#include "videooutsdl.h"
namespace LQF {
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

    int x = properties.GetProperty("x", static_cast<int>(SDL_WINDOWPOS_UNDEFINED));
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
    return RET_OK;
}

RET_CODE VideoOutSDL::Output(uint8_t *video_buf, uint32_t size)
{
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
    return RET_OK;
}

}
