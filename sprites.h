#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
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

void Sprite_handleCollidableX(Sprite* self);
void Sprite_handleCollidableY(Sprite* self);

void Sprite_update(Sprite* self,SDL_Renderer* renderer,float dt){
    self->rect.x+=self->vel_x * dt;
    Sprite_handleCollidableX(self);
    self->rect.y+=self->vel_y * dt;
    Sprite_handleCollidableY(self);
    SDL_RenderCopyF(renderer,self->texture,NULL,&self->rect);
}

int ResolveX(Sprite* self, Sprite* spr){
    float leftA   = self->rect.x;
    float rightA  = self->rect.x + self->rect.w;
    float topA    = self->rect.y;
    float bottomA = self->rect.y + self->rect.h;

    float leftB   = spr->rect.x;
    float rightB  = spr->rect.x + spr->rect.w;
    float topB    = spr->rect.y;
    float bottomB = spr->rect.y + spr->rect.h;


    float penetrationX = SDL_min(rightA, rightB) - SDL_max(leftA, leftB);
    float penetrationY = SDL_min(bottomA, bottomB) - SDL_max(topA, topB);


    int interX = (penetrationX > 0);
    int interY = (penetrationY > 0);  
    int resolveX = 0;
    int resolveY = 0;

    if (interX && interY){
        if (penetrationX < penetrationY){
            resolveX = 1;
        } else {
            resolveY = 1;
        }
    }  
    return resolveX;
}

void Sprite_handleCollidableX(Sprite* self){
    for (int i = 0; i < Vector_Size(self->sprites); i++){
        Sprite* spr = *(Sprite**)Vector_Get(self->sprites, i);
        if (!spr->collidable || spr == self) continue;

        if (SDL_HasIntersectionF(&self->rect, &spr->rect) && ResolveX(self, spr)){

            float m1 = fabs(self->weight * self->vel_x);
            float m2 = fabs(spr->weight  * spr->vel_x);

            if (fabs(spr->vel_x) < 0.01f) m2 = spr->weight * 1000;
            if (fabs(self->vel_x) < 0.01f) m1 = self->weight * 1000;

            if (m1 > m2){
                if (self->vel_x > 0){
                    spr->rect.x = self->rect.x + self->rect.w;
                } else if (self->vel_x < 0){
                    spr->rect.x = self->rect.x - spr->rect.w;
                }
            }
            else {
                if (spr->vel_x > 0){
                    self->rect.x = spr->rect.x + spr->rect.w;
                } else if (spr->vel_x < 0){
                    self->rect.x = spr->rect.x - self->rect.w;
                }
                self->vel_x = 0;
            }
        }
    }
}

SDL_Texture* LoadImage(const char* path,SDL_Renderer* renderer){
    SDL_Surface* surface=IMG_Load(path);
    if (!surface){
        emscripten_log(1,"Failed to load image: %s",IMG_GetError());
        return NULL;
    }
    SDL_Texture* texture=SDL_CreateTextureFromSurface(renderer,surface);
    SDL_FreeSurface(surface);
    if (!texture){
        emscripten_log(1,"Failed to create texture: %s",SDL_GetError());
        return NULL;
    }
    return texture;
}

void Sprite_handleCollidableY(Sprite* self){
    for (int i = 0; i < Vector_Size(self->sprites); i++){
        Sprite* spr = *(Sprite**)Vector_Get(self->sprites, i);
        if (!spr->collidable || spr == self) continue;

        float left   = SDL_max(self->rect.x, spr->rect.x);
        float right  = SDL_min(self->rect.x + self->rect.w, spr->rect.x + spr->rect.w);
        float top    = SDL_max(self->rect.y, spr->rect.y);
        float bottom = SDL_min(self->rect.y + self->rect.h, spr->rect.y + spr->rect.h);

        float px = right - left;
        float py = bottom - top;

        if (px > 0 && py > 0 && py <= px){

            float centerA = self->rect.y + self->rect.h * 0.5f;
            float centerB = spr->rect.y + spr->rect.h * 0.5f;

            if (centerA < centerB){
                self->rect.y -= py;
            } else {
                self->rect.y += py;
            }

            self->vel_y = 0;
        }
    }
}

void Sprite_destroy(Sprite* self){
    SDL_DestroyTexture(self->texture);
}
