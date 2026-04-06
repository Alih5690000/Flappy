#include <SDL2/SDL.h>
#include "vec.c"
typedef struct Sprite{
    SDL_Texture* texture;
    SDL_FRect rect;
    float vel_x;
    float vel_y;
    float* gravity;
    float weight;
    int collidable;
    int active;
    Vector* sprites;
    void (*update)(struct Sprite* self,SDL_Renderer* renderer,float dt);
    void (*destroy)(struct Sprite* self);
} Sprite;

typedef void(*SpriteUpdateFunc)(Sprite* self,SDL_Renderer* renderer,float dt);
typedef void(*SpriteDestroyFunc)(Sprite* self);

void Sprite_update(Sprite* self,SDL_Renderer* renderer,float dt){
    self->rect.x+=self->vel_x * dt;
    self->rect.y+=self->vel_y * dt;
    SDL_RenderCopyF(renderer,self->texture,NULL,&self->rect);
}

void Sprite_handleCollidable(Sprite* self){
    for (int i=0;i<Vector_Size(self->sprites);i++){
        Sprite* spr=*(Sprite**)Vector_Get(self->sprites,i);
        if (spr->collidable){
            SDL_FRect intersection;
            if (SDL_IntersectFRect(&self->rect,&spr->rect,&intersection)){
                if (self->vel_x>0){
                    self->rect.x=intersection.x-intersection.w;
                    self->vel_x=0;
                }
                else if (self->vel_x<0){
                    self->rect.x=intersection.x+intersection.w;
                    self->vel_x=0;
                }
                else{
                    if (spr->vel_x>0){
                        self->rect.x=intersection.x-intersection.w;
                    }
                    else if (spr->vel_x<0){
                        self->rect.x=intersection.x+intersection.w;
                    }
                }
                if (self->vel_y>0){
                    self->rect.y=intersection.y-intersection.h;
                    self->vel_y=0;
                }
                else if (self->vel_y<0){
                    self->rect.y=intersection.y+intersection.h;
                    self->vel_y=0;
                }
                else{
                    if (spr->vel_y>0){
                        self->rect.y=intersection.y-intersection.h;
                    }
                    else if (spr->vel_y<0){
                        self->rect.y=intersection.y+intersection.h;
                    }
                }
            }
        }
    }
}

void Sprite_destroy(Sprite* self){
    SDL_DestroyTexture(self->texture);
}
