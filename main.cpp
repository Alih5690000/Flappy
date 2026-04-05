#include <SDL2/SDL.h>
#include <emscripten.h>
#include "vec.c"

em_arg_callback_func currloop;

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* wintexture;
int running=1;

typedef struct Sprite{
    SDL_Texture* texture;
    SDL_FRect rect;
    float vel_x;
    float vel_y;
    float gravity;
    int collidable;
    Vector* sprites;
    void (*update)(struct Sprite* self,SDL_Renderer* renderer,float dt);
    void (*destroy)(struct Sprite* self);
} Sprite;

void Sprite_update(Sprite* self,SDL_Renderer* renderer,float dt){
    self->rect.x+=self->vel_x * dt;
    self->rect.y+=self->vel_y * dt;
    SDL_RenderCopyF(renderer,self->texture,NULL,&self->rect);
}

void Sprite_destroy(Sprite* self){
    SDL_DestroyTexture(self->texture);
}

typedef struct Player{
    Sprite base;
} Player;

void Player_update(Player* self,SDL_Renderer* renderer,float dt){
    const Uint8* keys= SDL_GetKeyboardState(NULL);
    if (keys[SDL_SCANCODE_SPACE]){
        self->base.vel_y=-300;
    }
    if (keys[SDL_SCANCODE_D]){
        self->base.vel_x=300;
    }
    if(keys[SDL_SCANCODE_A]){
        self->base.vel_x=-300;
    }
    self->base.vel_y+=self->base.gravity*dt;
    self->base.rect.x+=self->base.vel_x * dt;
    self->base.rect.y+=self->base.vel_y * dt;
    for (int i=0;i<Vector_Size(self->base.sprites);i++){
        Sprite* spr=*(Sprite**)Vector_Get(self->base.sprites,i);
        if (spr->collidable){
            SDL_FRect intersection;
            if (SDL_IntersectFRect(&self->base.rect,&spr->rect,&intersection)){
                if (self->base.vel_y>0){
                    self->base.rect.y=intersection.y-intersection.h;
                    self->base.vel_y=0;
                }
                if (self->base.vel_y<0){
                    self->base.rect.y=intersection.y+intersection.h;
                    self->base.vel_y=0;
                }
                if (self->base.vel_x>0){
                    self->base.rect.x=intersection.x-intersection.w;
                    self->base.vel_x=0;
                }
                if (self->base.vel_x<0){
                    self->base.rect.x=intersection.x+intersection.w;
                    self->base.vel_x=0;
                }
            }
        }
    }
}

typedef struct Scene1{
    Player plr;
    SDL_Renderer* renderer;
    SDL_Texture* wintexture;
    float dt;
    float gravity;
    Vector* sprites;
} Scene1;

void init1(Scene1* scene, SDL_Renderer* renderer,SDL_Texture* wintexture){
    scene->renderer=renderer;
    scene->wintexture=wintexture;
    scene->gravity=500;
    scene->sprites=CreateVector(sizeof(Sprite*));
    scene->dt=0;
    {
        SDL_Texture* plr_txt=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,100,100);
        SDL_FRect plr_rect={100,100,100,100};
        scene->plr.base={plr_txt,plr_rect,.update=Sprite_update,.destroy=Sprite_destroy,
            .collidable=0,.gravity=scene->gravity,.sprites=scene->sprites};
    }
}

void loop1(Scene1* scene){
    SDL_Event event;
        while(SDL_PollEvent(&event)){
            if(event.type==SDL_QUIT)
                running=0;
        }
        SDL_SetRenderTarget(scene->renderer,wintexture);
        {
            scene->plr.base.update(&scene->plr.base,scene->renderer,scene->dt);
        }
        SDL_SetRenderTarget(scene->renderer,NULL);
        SDL_RenderCopy(scene->renderer,scene->wintexture,NULL,NULL);
        SDL_RenderPresent(scene->renderer);
}

int main(){
    window=SDL_CreateWindow("Flappy",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        1000,800,
        SDL_WINDOW_SHOWN);
    renderer=SDL_CreateRenderer(window,-1,0);
    wintexture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,1000,800);
    Scene1 scene;
    init1(&scene, renderer,wintexture);
    int running=1;
    currloop=(em_arg_callback_func)loop1;
    emscripten_set_main_loop_arg(currloop, &scene, 0, 1);
}