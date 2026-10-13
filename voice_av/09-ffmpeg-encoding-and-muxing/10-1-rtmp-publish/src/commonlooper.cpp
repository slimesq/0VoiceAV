#include "commonlooper.h"
#include "dlog.h"
void *CommonLooper::trampoline(void *p)
{
    LogInfo("into");
    ((CommonLooper*)p)->SetRunning(true);
    ((CommonLooper*)p)->Loop();
     ((CommonLooper*)p)->SetRunning(false);
    LogInfo("leave");
    return NULL;
}

CommonLooper::CommonLooper() = default;

CommonLooper::~CommonLooper()
{
    Stop();
}

RET_CODE CommonLooper::Start()
{
    LogInfo("into");
    if(this->worker_) {
        return RET_FAIL;
    }
    this->request_abort_ = false;
    try {
        this->worker_ = new std::thread{trampoline, this};
    } catch(const std::exception &error) {
        LogError("Starting thread failed: %s", error.what());
        return RET_FAIL;
    }
    return RET_OK;
}

void CommonLooper::Stop()
{
    request_abort_ = true;
    if(worker_) {
        if(worker_->joinable()) {
            worker_->join();
        }
        delete worker_;
        worker_ = NULL;
    }
}

bool CommonLooper::Running()
{
    return running_;
}

void CommonLooper::SetRunning(bool running)
{
    running_ = running;
}


