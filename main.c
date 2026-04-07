#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <emscripten.h>
#include "vec.c"
#include "vid.h"
#include "sprites.h"
#include <stdlib.h>
#include <time.h>

typedef enum SPRITES{
    PLAYER,
    PIPE
} SPRITES;

em_arg_callback_func currloop;

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* wintexture;
TTF_Font* font;
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
    pipe->id=PIPE;
    pipe->passed=0;
    pipe->texture = pipe_txt;
    pipe->rect = rect;
    pipe->update=Wall_update;
    pipe->destroy=Sprite_destroy;
    pipe->active=1;
    pipe->collidable=0;
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
        self->base.rect=(SDL_FRect){100,100,50,50};
        self->base.vel_y=0;
        self->rotation=90.f;

    }
    self->base.vel_y+=*self->base.gravity * dt * self->base.weight;
    self->rotation=SDL_clamp((self->base.vel_y!=0?self->base.vel_y:1)/600,-1,1)*90;
    self->base.rect.y+=self->base.vel_y * dt;
    Sprite_handleCollidableY((Sprite*)self);
    int res=SDL_RenderCopyExF(renderer,self->base.texture,NULL,&self->base.rect,self->rotation
        ,&(SDL_FPoint){self->base.rect.w/2.f,self->base.rect.h/2.f},SDL_FLIP_NONE);
    if (res!=0){
        emscripten_log(1,"Render error %s",SDL_GetError());
    }
}

void Player_destroy(Player* self){
    SDL_DestroyTexture(self->base.texture);
}

typedef struct Scene1{
    SDL_Renderer* renderer;
    SDL_Texture* wintexture;
    SDL_Texture* bgtxt;
    SDL_FRect r1,r2;
    SDL_Texture* ground_txt;
    SDL_FRect g1,g2;
    Player* plr;
    float dt;
    int start,end;
    float gravity;
    float waiting;
    Vector* sprites;
    int GameOver;
    float score;
} Scene1;

Player* CreatePlayer(SDL_Renderer* renderer,float* gravity,Vector* sprites);

void reset1(Scene1* scene){
    scene->gravity=500;
    scene->GameOver=0;
    for (int i=Vector_Size(scene->sprites)-1;i>=0;i--){
        Sprite* spr=*(Sprite**)Vector_Get(scene->sprites,i);
        spr->destroy(spr);
        Vector_erase(scene->sprites,i);
    }
    {
        scene->plr->base.destroy(&scene->plr->base);
        Player* plr=CreatePlayer(scene->renderer,&scene->gravity,scene->sprites);
        scene->plr=plr;
        Vector_PushBack(scene->sprites,&plr);
    }
}

Player* CreatePlayer(SDL_Renderer* renderer,float* gravity,Vector* sprites){
    Player* plr=(Player*)malloc(sizeof(Player));
    SDL_Texture* txt=LoadImage("assets/Player.png",renderer);
    SDL_Texture* plr_txt=DeepCopyTexture(renderer,txt);
    SDL_SetTextureBlendMode(plr_txt,SDL_BLENDMODE_BLEND);
    SDL_DestroyTexture(txt);
    SDL_FRect plr_rect={100,100,50,50};
    plr->base = (Sprite){
        .texture = plr_txt,
        .rect = plr_rect,
        .vel_x = 0,
        .vel_y = 0,
        .gravity = gravity,
        .weight = 1.f,
        .collidable = 1,
        .active=1,
        .sprites = sprites,
        .update = (SpriteUpdateFunc)Player_update,
        .destroy = (SpriteDestroyFunc)Player_destroy
    };
    plr->base.id=PLAYER;
    plr->rotation=90.f;
    plr->base.update=(SpriteUpdateFunc)Player_update;
    plr->base.destroy=(SpriteDestroyFunc)Player_destroy;
    emscripten_log(1,"Player's update: %d",plr->base.update);
    emscripten_log(1,"Player_update: %d",Player_update);
    emscripten_log(1,"Player %d",plr);
    emscripten_log(1,"Player's base %d",&plr->base);
    return plr;
}

void init1(Scene1* scene, SDL_Renderer* renderer,SDL_Texture* wintexture){
    scene->renderer=renderer;
    scene->wintexture=wintexture;
    scene->gravity=500;
    scene->GameOver=0;
    scene->r1=(SDL_FRect){0,0,1000,800};
    scene->r2=(SDL_FRect){1000,0,1000,800};
    scene->g1=(SDL_FRect){0,700,1000,100};
    scene->g2=(SDL_FRect){1000,700,1000,100};
    scene->score=0;
    scene->sprites=CreateVector(sizeof(Sprite*));
    scene->bgtxt=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,1000,800);
    scene->ground_txt=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,1000,100);
    SDL_Texture* prev=SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer,scene->bgtxt);
    SDL_SetRenderDrawColor(renderer,155,155,255,255);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer,scene->ground_txt);
    SDL_SetRenderDrawColor(renderer,100,255,100,255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer,0,0,0,255);
    SDL_SetRenderTarget(renderer,prev);
    Vector_Resize(scene->sprites,500);
    scene->dt=0;
    {
        Player* plr=CreatePlayer(scene->renderer,&scene->gravity,scene->sprites);
        scene->plr=plr;
        Vector_PushBack(scene->sprites,&plr);
    }
}

void loop1(void* ptr){
    Scene1* scene=(Scene1*)ptr;
    scene->start=SDL_GetTicks();
    scene->dt=(scene->start-scene->end)/1000.f;
    scene->end=scene->start;
    scene->waiting+=scene->dt;
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        if(event.type==SDL_QUIT)
            running=0;
    }
    SDL_SetRenderTarget(scene->renderer,wintexture);
    SDL_SetRenderDrawColor(scene->renderer,0,0,0,255);
    SDL_RenderClear(scene->renderer);
    scene->r1.x-=100*scene->dt;
    scene->r2.x-=100*scene->dt;
    if (scene->r1.x<scene->r1.w*-1){
        scene->r1.x=scene->r2.x+scene->r2.w;
    }
    if (scene->r2.x<scene->r2.w*-1){
        scene->r2.x=scene->r1.x+scene->r1.w;
    }
    scene->r1.x-=100*scene->dt;
    scene->r2.x-=100*scene->dt;
    if (scene->g1.x<scene->g1.w*-1){
        scene->g1.x=scene->g2.x+scene->g2.w;
    }
    if (scene->g2.x<scene->g2.w*-1){
        scene->g2.x=scene->g1.x+scene->g1.w;
    }
    SDL_RenderCopyF(scene->renderer,scene->bgtxt,NULL,&scene->r1);
    SDL_RenderCopyF(scene->renderer,scene->bgtxt,NULL,&scene->r2);
    {
        if (!scene->GameOver && scene->waiting>5.f){
            scene->waiting=0.f;
            int y=rand()%400+200;
            SDL_FRect rect={1000,y,100,1000};
            Sprite* pipe=CreatePipe(rect,scene->sprites);
            Vector_PushBack(scene->sprites,&pipe);
            rect=(SDL_FRect){1000,y-800,100,600};
            pipe=CreatePipe(rect,scene->sprites);
            Vector_PushBack(scene->sprites,&pipe);
        }
        for (int i=Vector_Size(scene->sprites)-1;i>=0;i--){
            Sprite* spr=*(Sprite**)Vector_Get(scene->sprites,i);
            if (!spr->active){
                spr->destroy(spr);
                Vector_erase(scene->sprites,i);
            }
        }
        for (int i=0;i<Vector_Size(scene->sprites);i++){
            Sprite* spr=*(Sprite**)Vector_Get(scene->sprites,i);
            spr->update(spr,scene->renderer,scene->dt);
        }
        SDL_RenderCopyF(scene->renderer,scene->ground_txt,NULL,&scene->g1);
        SDL_RenderCopyF(scene->renderer,scene->ground_txt,NULL,&scene->g2);
        if (!scene->plr->base.active){
            scene->GameOver=1;
        }
        for (int i=0;i<Vector_Size(scene->sprites);i++){
            Sprite* spr=*(Sprite**)Vector_Get(scene->sprites,i);
            if (spr->id==PIPE && spr->active && 
                spr->rect.x + spr->rect.w < scene->plr->base.rect.x && 
                !scene->GameOver && !spr->passed && spr!=(Sprite*)scene->plr){
                scene->score+=0.5f;
                spr->passed=1;
                emscripten_log(1,"Score: %.1f",scene->score);
            }
        }
        if (scene->GameOver){
            SDL_SetRenderDrawColor(scene->renderer,0,0,0,155);
            SDL_RenderFillRect(scene->renderer,NULL);
        }
    }
    SDL_SetRenderTarget(scene->renderer,NULL);
    SDL_RenderCopy(scene->renderer,scene->wintexture,NULL,NULL);
    SDL_RenderPresent(scene->renderer);
}

int main(){
    srand(time(NULL));
    SDL_Init(SDL_INIT_EVERYTHING);
    IMG_Init(IMG_INIT_PNG);
    window=SDL_CreateWindow("Flappy",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        1000,800,
        SDL_WINDOW_SHOWN);
    renderer=SDL_CreateRenderer(window,-1,0);
    font=TTF_OpenFont("assets/Minecraft.ttf",24);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    wintexture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,1000,800);
    Scene1 scene;
    init1(&scene, renderer,wintexture);
    currloop=(em_arg_callback_func)loop1;
    emscripten_set_main_loop_arg(currloop, &scene, 0, 1);
}