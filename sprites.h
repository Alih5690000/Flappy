#include <SDL2/SDL.h>
#include "vec.c"
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
