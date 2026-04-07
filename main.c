#include <SDL2/SDL.h>
#include <emscripten.h>
#include "vec.c"
#include "sprites.h"
#include <stdlib.h>

em_arg_callback_func currloop;

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* wintexture;
int running=1;

void Wall_update(Sprite* self,SDL_Renderer* renderer,float dt){
    self->vel_x=-200;
    self->rect.x+=self->vel_x * dt;
    for (int i=0;i<Vector_Size(self->sprites);i++){
        Sprite* spr=*(Sprite**)Vector_Get(self->sprites,i);
        if (spr->active && spr!=self && SDL_HasIntersectionF(&self->rect, &spr->rect)){
            spr->active=0;
        }
    }
    SDL_RenderCopyF(renderer,self->texture,NULL,&self->rect);
}

Sprite* CreatePipe(SDL_FRect rect, Vector* sprites){
    Sprite* pipe=(Sprite*)malloc(sizeof(Sprite));
    SDL_Texture* pipe_txt=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,100,800);
    SDL_Texture* prev=SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer,pipe_txt);
    SDL_Rect idk={0,0,100,800};
    SDL_SetRenderDrawColor(renderer,0,255,0,255);
    SDL_RenderFillRect(renderer,&idk);
    SDL_SetRenderTarget(renderer,prev);
    pipe->texture = pipe_txt;
    pipe->rect = rect;
    pipe->update=Wall_update;
    pipe->destroy=Sprite_destroy;
    pipe->active=1;
    pipe->collidable=1;
    pipe->vel_x=-200;
    pipe->weight=9999.f;
    pipe->sprites=sprites;
    return pipe;
}

typedef struct Player{
    Sprite base;
    float rotation;
} Player;

void Player_update(Player* self,SDL_Renderer* renderer,float dt){
    const Uint8* keys= SDL_GetKeyboardState(NULL);
    if (keys[SDL_SCANCODE_SPACE]){
        self->base.vel_y=-300;
        self->rotation=0.f;
    }
    if (keys[SDL_SCANCODE_R]){
        emscripten_log(1,"Player rect: %f, %f, %f, %f",
        self->base.rect.x,self->base.rect.y,self->base.rect.w,self->base.rect.h);
    }
    if (keys[SDL_SCANCODE_E]){
        self->base.rect=(SDL_FRect){100,100,100,100};
        self->base.vel_y=0;
        self->rotation=90.f;

    }
    self->base.vel_y+=*self->base.gravity * dt * self->base.weight;
    self->rotation=SDL_clamp((self->base.vel_y!=0?self->base.vel_y:1)/600,-1,1)*90;
    self->base.rect.y+=self->base.vel_y * dt;
    Sprite_handleCollidableY((Sprite*)self);
    SDL_RenderCopyExF(renderer,self->base.texture,NULL,&self->base.rect,self->rotation
        ,&(SDL_FPoint){self->base.rect.w/2.f,self->base.rect.h/2.f},SDL_FLIP_NONE);
}

void Player_destroy(Player* self){
    SDL_DestroyTexture(self->base.texture);
}

typedef struct Scene1{
    SDL_Renderer* renderer;
    SDL_Texture* wintexture;
    float dt;
    int start,end;
    float gravity;
    Vector* sprites;
} Scene1;

void init1(Scene1* scene, SDL_Renderer* renderer,SDL_Texture* wintexture){
    scene->renderer=renderer;
    scene->wintexture=wintexture;
    scene->gravity=500;
    scene->sprites=CreateVector(sizeof(Sprite*));
    Vector_Resize(scene->sprites,500);
    scene->dt=0;
    {
        Player* plr=(Player*)malloc(sizeof(Player));
        SDL_Texture* plr_txt=LoadImage("assets/Player.png",scene->renderer);
        SDL_Texture* prev=SDL_GetRenderTarget(scene->renderer);
        SDL_SetRenderTarget(scene->renderer,plr_txt);
        SDL_Rect idk={0,75,100,25};
        SDL_SetRenderDrawColor(scene->renderer,255,0,255,255);
        SDL_RenderFillRect(scene->renderer,&idk);
        SDL_SetRenderTarget(scene->renderer,prev);
        SDL_FRect plr_rect={100,100,100,100};
        plr->base = (Sprite){
            .texture = plr_txt,
            .rect = plr_rect,
            .vel_x = 0,
            .vel_y = 0,
            .gravity = &scene->gravity,
            .weight = 1.f,
            .collidable = 1,
            .active=1,
            .sprites = scene->sprites,
            .update = (SpriteUpdateFunc)Player_update,
            .destroy = (SpriteDestroyFunc)Player_destroy
        };
        plr->rotation=90.f;
        plr->base.update=(SpriteUpdateFunc)Player_update;
        plr->base.destroy=(SpriteDestroyFunc)Player_destroy;
        emscripten_log(1,"Player's update: %d",plr->base.update);
        emscripten_log(1,"Player_update: %d",Player_update);
        emscripten_log(1,"Player %d",plr);
        emscripten_log(1,"Player's base %d",&plr->base);
        Vector_PushBack(scene->sprites,&plr);
    }
    Sprite* pipe=CreatePipe((SDL_FRect){500,500,100,300},scene->sprites);
    Vector_PushBack(scene->sprites,&pipe);
}

void loop1(void* ptr){
    Scene1* scene=(Scene1*)ptr;
    scene->start=SDL_GetTicks();
    scene->dt=(scene->start-scene->end)/1000.f;
    scene->end=scene->start;
    SDL_Event event;
        while(SDL_PollEvent(&event)){
            if(event.type==SDL_QUIT)
                running=0;
        }
        SDL_SetRenderTarget(scene->renderer,wintexture);
        SDL_SetRenderDrawColor(scene->renderer,155,155,255,255);
        SDL_RenderClear(scene->renderer);
        {
            for (int i=Vector_Size(scene->sprites)-1;i>=0;i--){
                Sprite* spr=*(Sprite**)Vector_Get(scene->sprites,i);
                if (!spr->active){
                    spr->destroy(spr);
                    Vector_erase(scene->sprites,i);
                }
            }
            emscripten_log(1,"Sprites count: %d",Vector_Size(scene->sprites));
            for (int i=0;i<Vector_Size(scene->sprites);i++){
                Sprite* spr=*(Sprite**)Vector_Get(scene->sprites,i);
                spr->update(spr,scene->renderer,scene->dt);
            }
        }
        SDL_SetRenderTarget(scene->renderer,NULL);
        SDL_RenderCopy(scene->renderer,scene->wintexture,NULL,NULL);
        SDL_RenderPresent(scene->renderer);
}

int main(){
    SDL_Init(SDL_INIT_EVERYTHING);
    IMG_Init(IMG_INIT_PNG);
    window=SDL_CreateWindow("Flappy",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        1000,800,
        SDL_WINDOW_SHOWN);
    renderer=SDL_CreateRenderer(window,-1,0);
    wintexture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,1000,800);
    Scene1 scene;
    init1(&scene, renderer,wintexture);
    currloop=(em_arg_callback_func)loop1;
    emscripten_set_main_loop_arg(currloop, &scene, 0, 1);
}