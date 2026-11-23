#include "videooutputloop.h"
#include "dlog.h"
#include "avsync.h"
#include "avtimebase.h"
namespace LQF {
VideoOutputLoop::VideoOutputLoop(AVSync *avsync, PacketQueue *packet_queue, FrameQueue *frame_queue)
    :CommonLooper(),
      avsync_(avsync),
      packet_queue_(packet_queue),
      frame_queue_(frame_queue)
{

}

RET_CODE VideoOutputLoop::Init(const Properties &properties)
{
    if(!avsync_ || !frame_queue_ || !packet_queue_) {
        LogError("avsync_ or packet_queue_ or frame_queue_ is null");
        return RET_FAIL;
    }
    return RET_OK;
}
// 计算上一帧需要持续的duration，这里有校正算法
static double vp_duration(Frame *vp, Frame *nextvp, float playback_rate) {
    if (vp->serial == nextvp->serial) { // 同一播放序列
        double duration = nextvp->pts - vp->pts;
        if (isnan(duration) // duration 数值异常
                || duration <= 0    // pts值没有递增时
                || duration > 3600    // 超过了最大帧范围
                ) {
            return vp->duration/playback_rate;	 /* 异常时以帧时间为基准(1秒/帧率) */
        }
        else {
            return duration/playback_rate;
        }
    } else {        // 不同播放序列
        return 0.0;
    }
}

/**
 * @brief 计算正在显示帧需要持续播放的时间。
 * @param delay 该参数实际传递的是当前显示帧和待播放帧的间隔。
 * @param is
 * @return 返回当前显示帧要持续播放的时间。为什么要调整返回的delay？为什么不支持使用相邻间隔帧时间？
 */
double VideoOutputLoop::compute_target_delay(double delay)
{
    double sync_threshold, diff = 0;
    double max_frame_duration = 3600;
    /* update delay to follow master synchronisation source */
    /* 如果发现当前主Clock源不是video，则计算当前视频时钟与主时钟的差值 */
    if (avsync_->get_master_sync_type() != AV_SYNC_VIDEO_MASTER) {
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */
        diff = avsync_->vidclk.get_clock() - avsync_->get_master_clock();
        double vpts = avsync_->vidclk.get_clock();
        double masterpts = avsync_->get_master_clock();
//        LogInfo("getvideo pts:%lf, mast clk:%lf", vpts, masterpts);
        /* skip or repeat frame. We take into account the
           delay to compute the threshold. I still don't know
           if it is the best guess */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN,
                               FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        if (!isnan(diff) && fabs(diff) < max_frame_duration) { // diff在最大帧duration内
            if (diff <= -sync_threshold) {      // 视频已经落后了
                delay = FFMAX(0, delay + diff); //
            }
            else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD) {
                // 视频超前
                //AV_SYNC_FRAMEDUP_THRESHOLD是0.1，此时如果delay>0.1, 如果2*delay时间就有点久
                delay = delay + diff;
            }
            else if (diff >= sync_threshold) {
                delay = 2 * delay; // 保持在 2 * AV_SYNC_FRAMEDUP_THRESHOLD内, 即是2*0.1 = 0.2秒内
            } else {
                // 其他条件就是delay = delay; 维持原来的delay, 依靠frame_timer+duration和当前时间进行对比
            }
        }
    } else {
        // 如果是以video为同步，则直接返回last_duration
    }

    LogDebug("video: delay=%0.3f A-V=%f\n",  delay, -diff);

    return delay;
}
void VideoOutputLoop::update_video_pts(double pts, int64_t pos, int serial) {
    /* update current video pts */
    avsync_->vidclk.set_clock(pts, serial);
//    avsync_->extclk.sync_clock_to_slave(&avsync_->vidclk);
}
void VideoOutputLoop::video_refresh(double *remaining_time)
{
    double time;

    Frame *vp = NULL, *lastvp = NULL;
retry:
    if (frame_queue_->frame_queue_nb_remaining() == 0) {// 所有帧已显示
        // nothing to do, no picture to display in the queue
        // 什么都不做，队列中没有图像可显示
    } else {
        double last_duration, duration, delay;


        /* dequeue the picture */
        // 从队列取出上一个Frame
        lastvp = frame_queue_->frame_queue_peek_last();// 上一帧：上次在显示的帧
        vp = frame_queue_->frame_queue_peek();  // 读取待显示的帧
        // lastvp 上一帧(正在显示的帧)
        // vp 等待显示的帧

        if (vp->serial != packet_queue_->serial()) {
            // 如果不是最新的播放序列，则将其从队列，以尽快读取最新的播放序列的Frame
            frame_queue_->frame_queue_next();
            goto retry;
        }

        if (lastvp->serial != vp->serial) {
            // 新的播放序列重置当前时间
            frame_timer = av_gettime_relative() / 1000000.0;
        }

        if (paused)
        {
            goto display;
            printf("视频暂停paused");
        }
        /*
             * last_duration 计算上一帧应显示的时长
             */
        last_duration = vp_duration(lastvp, vp, playback_rate_);

        /* 经过compute_target_delay方法，计算出待显示帧vp需要等待的时间 */
        // 如果以video同步，则delay直接等于last_duration。
        // 如果以audio或外部时钟同步，则需要比对主时钟调整待显示帧vp要等待的时间。
        delay = compute_target_delay(last_duration);

        time= av_gettime_relative()/1000000.0;
        /* frame_timer 实际上就是上一帧lastvp的播放时间,
             * frame_timer + delay 是待显示帧vp该播放的时间
             * */
        if (!b_first_frame && time < frame_timer + delay) { //判断是否继续显示上一帧
            // 当前系统时刻还未到达上一帧的结束时刻，那么还应该继续显示上一帧。
            // 计算出最小等待时间
            *remaining_time = FFMIN(frame_timer + delay - time, *remaining_time);
            goto display;
        }

        // 走到这一步，说明已经到了或过了该显示的时间，待显示帧vp的状态变更为当前要显示的帧

        frame_timer += delay;   // 更新当前帧播放的时间
        if (delay > 0 && time - frame_timer > AV_SYNC_THRESHOLD_MAX) {
            frame_timer = time; //如果和系统时间差距太大，就纠正为系统时间
        }
        SDL_LockMutex(frame_queue_->mutex_);
        if (!isnan(vp->pts))
            update_video_pts(vp->pts, vp->pos, vp->serial); // 更新video时钟
        SDL_UnlockMutex(frame_queue_->mutex_);

        if (!b_first_frame && frame_queue_->frame_queue_nb_remaining() > 1) {//只有有nextvp才会分析是否有必要检测是否该丢帧
            Frame *nextvp = frame_queue_->frame_queue_peek_next();
            duration = vp_duration(vp, nextvp, playback_rate_);
            if(avsync_->get_master_sync_type() != AV_SYNC_VIDEO_MASTER // 非视频同步方式
                    && time > frame_timer + duration) // 确实落后了一帧数据
            {
                LogInfo("dif:%lfs, drop frame\n",
                       (frame_timer + duration) - time);
                frame_drops_late++;             // 统计丢帧情况
                frame_queue_->frame_queue_next();       // 帧出队列真正丢掉
                goto retry;
            }
        }

        frame_queue_->frame_queue_next();   // 当前vp帧出队列
        force_refresh = 1;          /* 说明需要刷新视频帧 */

    }
display:
    /* display picture */
    if (force_refresh && frame_queue_->rindex_shown_) {
        if(vp) {
            video_refresh_callback_(vp);
            b_first_frame = false;
        }
    }
    force_refresh = 0;
}

void VideoOutputLoop::Loop()
{
    double remaining_time = 0.0;
    while(true)
    {
        if(request_exit_)
            break;
        /*
         * remaining_time就是用来进行音视频同步的。
         * 在video_refresh函数中，根据当前帧显示时刻(display time)和实际时刻(actual time)计算需要sleep的时间，保证帧按时显示
         */
        if(remaining_time > REFRESH_RATE)
            remaining_time = REFRESH_RATE;
        if (remaining_time > 0.0)
            av_usleep((int64_t)(remaining_time * 1000000.0));
        remaining_time = REFRESH_RATE;

        video_refresh(&remaining_time);
    }
    LogWarn("Loop exit");
}



}
