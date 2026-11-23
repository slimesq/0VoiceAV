#include "videooutsdl.h"
namespace LQF {

// 自定义SDL事件
#define FRAME_REFRESH_EVENT (SDL_USEREVENT+1)

VideoOutSDL::VideoOutSDL()
{

}

VideoOutSDL::~VideoOutSDL()
{
    if(texture)
        SDL_DestroyTexture(texture);
    if(renderer)
        SDL_DestroyRenderer(renderer);
    if(win)
        SDL_DestroyWindow(win);
    if(mutex)
        SDL_DestroyMutex(mutex);

    SDL_Quit();
}

RET_CODE VideoOutSDL::Init(const Properties &properties)
{

    //初始化 SDL
    if(SDL_Init(SDL_INIT_VIDEO))
    {
        LogError("Could not initialize SDL - %s", SDL_GetError());
        return RET_FAIL;
    }

    int x = properties.GetProperty("win_x", static_cast<int>(SDL_WINDOWPOS_UNDEFINED));
    video_width = properties.GetProperty("video_width", 320);
    video_height = properties.GetProperty("video_height", 240);
    //创建窗口
    win = SDL_CreateWindow("Simplest YUV Player",
                           x,
                           SDL_WINDOWPOS_UNDEFINED,
                           video_width, video_height,
                           SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(!win)
    {
        LogError("SDL: could not create window, err:%s",SDL_GetError());
        return RET_FAIL;
    }

    // 基于窗口创建渲染器
    renderer = SDL_CreateRenderer(win, -1, 0);
    if(!renderer)
    {
        LogError("SDL: could not create renderer, err:%s",SDL_GetError());
        return RET_FAIL;
    }
    // 基于渲染器创建纹理
    texture = SDL_CreateTexture(renderer,
                                pixformat,
                                SDL_TEXTUREACCESS_STREAMING,
                                video_width,
                                video_height);
    if(!texture)
    {
        LogError("SDL: could not create texture, err:%s",SDL_GetError());
        return RET_FAIL;
    }
    video_buf_size_ = video_width * video_height * 1.5;
    video_buf_ = (uint8_t *)malloc(video_buf_size_); // 缓存要显示的画面
    mutex = SDL_CreateMutex();
    return RET_OK;
}

RET_CODE VideoOutSDL::Cache(uint8_t *video_buf, uint32_t size)
{
    SDL_LockMutex(mutex);
    memcpy(video_buf_, video_buf, video_buf_size_);
    SDL_UnlockMutex(mutex);
    SDL_Event event;
    event.type = FRAME_REFRESH_EVENT;
    SDL_PushEvent(&event);
    return RET_OK;
}

RET_CODE VideoOutSDL::Output(uint8_t *video_buf, uint32_t size)
{
    SDL_LockMutex(mutex);
    //    return RET_OK;
    // 设置纹理的数据
    SDL_UpdateTexture(texture, NULL, video_buf, video_width);

    //FIX: If window is resize
    rect.x = 0;
    rect.y = 0;
    rect.w = video_width;
    rect.h = video_height;

    // 清除当前显示
    SDL_RenderClear(renderer);
    // 将纹理的数据拷贝给渲染器
    SDL_RenderCopy(renderer, texture, NULL, &rect);
    // 显示
    SDL_RenderPresent(renderer);
    SDL_UnlockMutex(mutex);
    return RET_OK;
}

RET_CODE VideoOutSDL::Loop()
{
    while(1)      // 主循环
    {
//        LogInfo("into");
        if(SDL_WaitEvent(&event) != 1)
            continue;

        switch(event.type)
        {
        case SDL_KEYDOWN:
            if(event.key.keysym.sym == SDLK_ESCAPE)
                return RET_OK;
            if(event.key.keysym.sym == SDLK_SPACE)
                return RET_OK;
            break;

        case SDL_QUIT:    /* Window is closed */
            return RET_OK;
            break;
        case FRAME_REFRESH_EVENT:
            Output(video_buf_,  video_buf_size_);
            break;
        default:
//            printf("unknow sdl event.......... event.type = %x\n", event.type);
            break;
        }
    }
}
}
