#include <SDL.h>
#include <SDL_ttf.h>
#include <emscripten/emscripten.h>
#include "vec.c"
#include "vid.h"
#include "sprites.h"
#include <stdlib.h>
#include <time.h>

void DullFunc(){}

typedef enum SPRITES{
    PLAYER,
    PIPE
} SPRITES;

em_arg_callback_func currloop;

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* wintexture;
TTF_Font* font;
int maxScore;
int running=1;

void Load_(){
    FILE* file = fopen("/data/maxscore.txt", "r");
    if (file) {
        fscanf(file, "%d", &maxScore);
        fclose(file);
    } else {
        maxScore = 0;
    }
    emscripten_log(1, "Max Score: %d", maxScore);
}

void Load(){
    EM_ASM(
        FS.mkdir("/data");
        FS.mount(IDBFS, {}, "/data");
        FS.syncfs(true, function(err) {
            if (err) {
                console.error("Error syncing filesystem:", err);
            } else {
                console.log("Filesystem synced successfully.");
                _Load_();
            }
        });
    );
}

void Write(){
    FILE* file = fopen("/data/maxscore.txt", "w");
    if (file) {
        fprintf(file, "%d", maxScore);
        fclose(file);
    } else {
        emscripten_log(1, "Error opening file for writing.");
    }
    EM_ASM(
        FS.syncfs(false,function(err) {
            if (err) {
                console.error("Error syncing filesystem:", err);
            } else {
                console.log("Filesystem synced successfully.");
            }
        });
    );
}

void ScaleRect(SDL_FRect* rect,float scaleX,float scaleY){
    rect->w*=scaleX;
    rect->h*=scaleY;
    rect->x-=rect->w*(scaleX-1)/2.f;
    rect->y-=rect->h*(scaleY-1)/2.f;
}

const float speed=300.f;

void Wall_update(Sprite* self,SDL_Renderer* renderer,float dt){
    self->vel_x=-speed;
    self->rect.x+=self->vel_x * dt;
    for (int i=0;i<Vector_Size(self->sprites);i++){
        Sprite* spr=*(Sprite**)Vector_Get(self->sprites,i);
        if (spr->alive && spr!=self && SDL_HasIntersectionF(&self->rect, &spr->rect)){
            spr->alive=0;
        }
    }
    float scale = 3.0f;

    SDL_FRect drawRect = {
        .w = self->rect.w * scale,
        .h = self->rect.h,
        .x = self->rect.x - (self->rect.w * scale - self->rect.w) / 2.0f,
        .y =(self->flip?self->rect.y+35:self->rect.y-60)
    };
    SDL_SetRenderDrawColor(renderer,255,0,0,100);
    SDL_RenderDrawRectF(renderer,&self->rect);
    if (!self->flip){
        SDL_RenderCopyExF(renderer,self->texture,NULL,&drawRect,0,NULL,SDL_FLIP_NONE);
    }
    else {
        SDL_RenderCopyExF(renderer,self->texture,NULL,&drawRect,0,NULL,SDL_FLIP_VERTICAL);
    }
}

void Wall_destroy(Sprite* s){}

SDL_Texture* saw_txt_cache;

typedef struct Saw{
    Sprite base;
    float angle;
} Saw;

void Saw_update(Saw* self,SDL_Renderer* renderer,float dt){
    emscripten_log(1,"Saw update rect is %f, %f, %f, %f",
        self->base.rect.x,self->base.rect.y,self->base.rect.w,self->base.rect.h);
    if (self->base.rect.x+self->base.rect.w<0 && self->base.vel_x<0){
        self->base.vel_x=-self->base.vel_x;
        if (self->base.rect.y+100>700){
            self->base.rect.y-=100;
        }
        else{
            self->base.rect.y+=100;
        }
    }
    if (self->base.vel_x>0)
        self->angle+=dt*200.f;
    else if (self->base.vel_x<0)
        self->angle-=dt*200.f;
    self->base.rect.x+=self->base.vel_x*dt;
    for (int i=0;i<Vector_Size(self->base.sprites);i++){
        Sprite* spr=*(Sprite**)Vector_Get(self->base.sprites,i);
        if (spr->alive && spr!=&self->base && SDL_HasIntersectionF
        (&self->base.rect, &spr->rect)){
            spr->alive=0;
        }
    }
    SDL_RenderCopyExF(renderer,self->base.texture,NULL,&self->base.rect,
        self->angle,&(SDL_FPoint){self->base.rect.w/2.f,self->base.rect.h/2.f},
        SDL_FLIP_NONE);
}

void Saw_destroy(Saw* self){
    
}

void CreateSaw(float x,float y,float* gravity,Vector* sprites){
    Saw* saw=malloc(sizeof(Saw));
    saw->base.texture=saw_txt_cache;
    saw->base.rect=(SDL_FRect){x,y,50,50};
    saw->base.vel_x=-400;
    saw->base.vel_y=0;
    saw->base.gravity=gravity;
    saw->base.weight=1.0f;
    saw->base.collidable=0;
    saw->base.active=1;
    saw->base.alive=0;
    saw->base.sprites=sprites;
    saw->base.update=(SpriteUpdateFunc)Saw_update;
    saw->base.destroy=(SpriteDestroyFunc)Saw_destroy;
    Vector_PushBack(sprites,&saw);
}

SDL_Texture* projectile_txt_cache;

typedef struct Projectile{
    Sprite base;
} Projectile;

void Projectile_update(Projectile* self,SDL_Renderer* renderer,float dt){
    self->base.rect.x+=self->base.vel_x * dt;
    self->base.rect.y+=self->base.vel_y * dt;
    if (self->base.rect.x<0 || self->base.rect.x>1000 || self->base.rect.y<0 || self->base.rect.y>800){
        self->base.active=0;
    }
    for (int i=0;i<Vector_Size(self->base.sprites);i++){
        Sprite* spr=*(Sprite**)Vector_Get(self->base.sprites,i);
        if (spr->alive && spr!=&self->base && SDL_HasIntersectionF
        (&self->base.rect, &spr->rect)){
            spr->alive=0;
            self->base.active=0;
        }
    }
    SDL_RenderCopyF(renderer,self->base.texture,NULL,&self->base.rect);
}

void Projectile_destroy(Projectile* self){}

void CreateProjectile(float x,float y,float vel_x,float vel_y,Vector* sprites){
    Projectile* proj=malloc(sizeof(Projectile));
    proj->base.texture=projectile_txt_cache;
    proj->base.rect=(SDL_FRect){x,y,10,10};
    proj->base.vel_x=vel_x;
    proj->base.vel_y=vel_y;
    proj->base.gravity=NULL;
    proj->base.weight=0.f;
    proj->base.collidable=0;
    proj->base.active=1;
    proj->base.alive=1;
    proj->base.sprites=sprites;
    proj->base.update=(SpriteUpdateFunc)Projectile_update;
    proj->base.destroy=(SpriteDestroyFunc)Projectile_destroy;
    Vector_PushBack(sprites,&proj);
}

typedef struct LaserBeam{
    Sprite base;
    float timeWarning;
} LaserBeam;

void LaserBeam_destroy(LaserBeam* self){}

void LaserBeam_update(LaserBeam* self,SDL_Renderer* renderer,float dt){
    self->timeWarning+=dt;
    if (self->timeWarning<1.5f){
        SDL_FRect dangerRect=(SDL_FRect){
            0,self->base.rect.y,1000,20
        };
        SDL_SetRenderDrawColor(renderer,255,0,0,100);
        SDL_RenderFillRectF(renderer,&dangerRect);
        return;
    }
    if (self->base.rect.x>0){
        self->base.rect.x-=2000*dt;
    }
    else{
        self->base.rect.x=1000;
        if (self->base.rect.w>1000){
            self->base.rect.w-=2000*dt;
        }
        else{
            self->base.rect.x=1000;
            self->base.active=0;
        }
    }
    for (int i=0;i<Vector_Size(self->base.sprites);i++){
        Sprite* spr=*(Sprite**)Vector_Get(self->base.sprites,i);
        if (spr->alive && spr!=&self->base && SDL_HasIntersectionF(&self->base.rect, &spr->rect)){
            spr->alive=0;
        }
    }
    SDL_SetRenderDrawColor(renderer,255,0,0,255);
    SDL_RenderFillRectF(renderer,&self->base.rect);
}

Sprite* CreateLaserBeam(SDL_FRect rect,Vector* sprites){
    LaserBeam* beam=(LaserBeam*)malloc(sizeof(LaserBeam));
    beam->base.texture=NULL;
    beam->base.rect=rect;
    beam->base.vel_x=0;
    beam->base.vel_y=0;
    beam->base.gravity=NULL;
    beam->base.weight=0.f;
    beam->timeWarning=0.f;
    beam->base.collidable=0;
    beam->base.active=1;
    beam->base.alive=1;
    beam->base.sprites=sprites;
    beam->base.update=(SpriteUpdateFunc)LaserBeam_update;
    beam->base.destroy=(SpriteDestroyFunc)LaserBeam_destroy;
    return (Sprite*)beam;
}

SDL_Texture* pipe_txt_cache;

Sprite* CreatePipe(SDL_FRect rect, Vector* sprites,int flip){
    Sprite* pipe=(Sprite*)malloc(sizeof(Sprite));
    SDL_Texture* pipe_txt=pipe_txt_cache;
    pipe->id=PIPE;
    pipe->passed=0;
    pipe->flip=flip;
    pipe->texture = pipe_txt;
    pipe->rect = rect;
    pipe->update=Wall_update;
    pipe->destroy=Wall_destroy;
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
    int pressed;
} Player;

SDL_Texture* plr_txt_cache;

void Player_update(Player* self,SDL_Renderer* renderer,float dt){
    if (!self->base.alive){
        self->base.active=0;
        return;
    }
    const Uint8* keys= SDL_GetKeyboardState(NULL);
    if (keys[SDL_SCANCODE_SPACE] && !self->pressed){
        self->base.vel_y=-250;
        self->rotation=0.f;
        self->pressed=1;
    }
    if (!keys[SDL_SCANCODE_SPACE]){
        self->pressed=0;
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
    if (self->base.rect.y<0 || self->base.rect.y + self->base.rect.h > 700){
        self->base.active=0;
    }
    self->base.vel_y+=*self->base.gravity * dt * self->base.weight;
    self->rotation=SDL_clamp((self->base.vel_y!=0?self->base.vel_y:1)/600,-1,1)*90;
    self->base.rect.y+=self->base.vel_y * dt;
    Sprite_handleCollidableY((Sprite*)self);
    SDL_FRect drawRect=self->base.rect;
    ScaleRect(&drawRect,1.5f,1.5f);
    drawRect.y+=10;
    drawRect.x+=10;
    int res=SDL_RenderCopyExF(renderer,self->base.texture,NULL,&drawRect,self->rotation
        ,&(SDL_FPoint){drawRect.w/2.f,drawRect.h/2.f},SDL_FLIP_NONE);
    SDL_SetRenderDrawColor(renderer,0,255,0,100);
    SDL_RenderDrawRectF(renderer,&self->base.rect);
    if (res!=0){
        emscripten_log(1,"Render error %s",SDL_GetError());
    }
}

void Player_destroy(Player* self){

}

typedef struct PlayerCorpse{
    Sprite base;
    float angle;
} PlayerCorpse;

void PlayerCorpse_update(PlayerCorpse* self,SDL_Renderer* renderer,float dt){
    emscripten_log(1,"PlayerCorpse update rect is %f, %f, %f, %f",
        self->base.rect.x,self->base.rect.y,self->base.rect.w,self->base.rect.h);
    self->angle+=200 * dt;
    self->angle=(int)self->angle%360;
    self->base.rect.y+=self->base.vel_y * dt;
    self->base.vel_y+=*self->base.gravity * dt * self->base.weight;
    emscripten_log(1,"PlayerCorpse vel_y: %f",self->base.vel_y);
    SDL_FRect drawRect=self->base.rect;
    ScaleRect(&drawRect,1.5f,1.5f);
    SDL_RenderCopyExF(renderer,self->base.texture,NULL,&drawRect,self->angle
        ,&(SDL_FPoint){self->base.rect.w/2.f,self->base.rect.h/2.f},SDL_FLIP_NONE);
    if (self->base.rect.y>800){
        self->base.active=0;
    }
}

typedef struct Scene1{
    SDL_Renderer* renderer;
    SDL_Texture* wintexture;
    SDL_Texture* bgtxt;
    SDL_FRect r1,r2;
    SDL_Texture* ground_txt;
    SDL_Texture* score_txt;
    SDL_FRect g1,g2;
    Player* plr;
    float dt;
    int has_corpse;
    SDL_FRect gameOverRect;
    SDL_Texture* gameOver_txt;
    SDL_Texture* tryAgain_txt;
    int start,end;
    float gravity;
    float waiting;
    Vector* sprites;
    int GameOver;
    float lastScore;
    float score;
} Scene1;

Player* CreatePlayer(SDL_Renderer* renderer,float* gravity,Vector* sprites);

void reset1(Scene1* scene){
    scene->gravity=500;
    scene->GameOver=0;
    scene->has_corpse=0;
    scene->score=0;
    scene->gameOverRect=(SDL_FRect){500,-100,300,100};
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
    SDL_Texture* plr_txt=plr_txt_cache;
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
    plr->base.alive=1;
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
    scene->has_corpse=0;
    {
        SDL_Surface* surf=TTF_RenderText_Solid(font,"Game Over",(SDL_Color){255,255,255,255});
        scene->gameOver_txt=SDL_CreateTextureFromSurface(scene->renderer,surf);
        SDL_FreeSurface(surf);
    }
    {
        SDL_Surface* surf=TTF_RenderText_Solid(font,"Press R for try again",(SDL_Color){255,255,255,255});
        scene->tryAgain_txt=SDL_CreateTextureFromSurface(scene->renderer,surf);
        SDL_FreeSurface(surf);
    }
    scene->gameOverRect=(SDL_FRect){500,-100,300,100};
    {
        char buff[256];
        snprintf(buff, sizeof(buff), "Score: %d", (int)scene->score);
        SDL_Surface* surf=TTF_RenderText_Solid(font,buff,(SDL_Color){0,0,0,255});
        if (!surf){
            emscripten_log(1,"Error creating surface: %s",TTF_GetError());
            _Exit(1);
        }
        if (scene->score_txt){
            SDL_DestroyTexture(scene->score_txt);
        }
        scene->score_txt=SDL_CreateTextureFromSurface(renderer,surf);
        SDL_FreeSurface(surf);
    }
    scene->r1=(SDL_FRect){0,0,1000,800};
    scene->r2=(SDL_FRect){1000,0,1000,800};
    scene->g1=(SDL_FRect){0,700,1000,100};
    scene->g2=(SDL_FRect){1000,700,1000,100};
    scene->score=0;
    scene->sprites=CreateVector(sizeof(Sprite*));
    scene->bgtxt=LoadImage("assets/bg.jpg",renderer);
    scene->ground_txt=LoadImage("assets/ground.jpg",renderer);
    Vector_Resize(scene->sprites,500);
    scene->dt=0;
    {
        Player* plr=CreatePlayer(scene->renderer,&scene->gravity,scene->sprites);
        scene->plr=plr;
        Vector_PushBack(scene->sprites,&plr);
    }
}

void loop1(void* ptr){
    emscripten_log(1,"Loop start");
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
    const Uint8* keys=SDL_GetKeyboardState(NULL);
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
    scene->g1.x-=speed*scene->dt;
    scene->g2.x-=speed*scene->dt;
    if (scene->g1.x<scene->g1.w*-1){
        scene->g1.x=scene->g2.x+scene->g2.w;
    }
    if (scene->g2.x<scene->g2.w*-1){
        scene->g2.x=scene->g1.x+scene->g1.w;
    }
    SDL_RenderCopyF(scene->renderer,scene->bgtxt,NULL,&scene->r1);
    SDL_RenderCopyF(scene->renderer,scene->bgtxt,NULL,&scene->r2);
    {
        if (!scene->GameOver && scene->waiting>1.5f){
            scene->waiting=0.f;
            int y=rand()%300+200;
            SDL_FRect rect={1000,y,100,900};
            Sprite* pipe=CreatePipe(rect,scene->sprites,0);
            Vector_PushBack(scene->sprites,&pipe);
            rect=(SDL_FRect){1000,y-800,100,600};
            pipe=CreatePipe(rect,scene->sprites,1);
            Vector_PushBack(scene->sprites,&pipe);
            int vars=1;
            if (scene->score>=5){
                vars=2;
            }
            if (scene->score>=10){
                vars=3;
            }
            if (scene->score>=15){
                vars=4;
            }
            int chance=rand()%vars;
            if (chance==1){
                SDL_FRect laserRect={1000,rand()%400+200,900,20};
                Sprite* laser=CreateLaserBeam(laserRect,scene->sprites);
                Vector_PushBack(scene->sprites,&laser);
            }
            if (chance==2){
                CreateProjectile(1000,rand()%800, -500, rand()%200-100, scene->sprites);
            }
            if (chance==0){
                emscripten_log(1,"Creating saw");
                CreateSaw(1000,rand()%500+200,&scene->gravity,scene->sprites);
            }
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
            if (spr->update==Saw_update){
                emscripten_log(1,"Saw rect: %f, %f, %f, %f",
                    spr->rect.x,spr->rect.y,spr->rect.w,spr->rect.h);
            }
        }
        SDL_RenderCopyF(scene->renderer,scene->ground_txt,NULL,&scene->g1);
        SDL_RenderCopyF(scene->renderer,scene->ground_txt,NULL,&scene->g2);
        if (!scene->plr->base.active){
            if (scene->score>maxScore){
                maxScore=(int)scene->score;
                Write();
            }
            if (!scene->has_corpse){
                scene->has_corpse=1;
                PlayerCorpse* a=malloc(sizeof(PlayerCorpse));
                *a=(PlayerCorpse){
                    .base.texture=scene->plr->base.texture,
                    .base.rect=scene->plr->base.rect,
                    .base.vel_y=-400,
                    .base.vel_x=100,
                    .base.gravity=scene->plr->base.gravity,
                    .base.weight=scene->plr->base.weight,
                    .base.collidable=0,
                    .base.active=1,
                    .base.alive=1,
                    .base.sprites=scene->sprites,
                    .base.update=(SpriteUpdateFunc)PlayerCorpse_update,
                    .base.destroy=(SpriteDestroyFunc)Player_destroy,
                    .angle=0
                };
                Vector_PushBack(scene->sprites,&a);
                scene->GameOver=1;
            }
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
        if (scene->lastScore!=(int)scene->score){
            scene->lastScore=(int)scene->score;
            char buff[256];
            snprintf(buff, sizeof(buff), "Score: %d", (int)scene->score);
            SDL_Surface* surf=TTF_RenderText_Solid(font,buff,(SDL_Color){0,0,0,255});
            if (!surf){
                emscripten_log(1,"Error creating surface: %s",TTF_GetError());
                _Exit(1);
            }
            if (scene->score_txt){
                SDL_DestroyTexture(scene->score_txt);
            }
            scene->score_txt=SDL_CreateTextureFromSurface(scene->renderer,surf);
            SDL_FreeSurface(surf);
        }
        scene->lastScore=(int)scene->score;
        int w, h;
        SDL_QueryTexture(scene->score_txt, NULL, NULL, &w, &h);

        SDL_FRect dst = {10, 10, (float)w, (float)h};
        SDL_RenderCopyF(scene->renderer, scene->score_txt, NULL, &dst);
        if (scene->GameOver){
            int draw=0;
            if (scene->gameOverRect.y<350){
                scene->gameOverRect.y+=200 * scene->dt;
            }
            else if(scene->gameOverRect.y>=350){
                draw=1;
                scene->gameOverRect.y=350;
            }
            if (keys[SDL_SCANCODE_R]){
                reset1(scene);
            }
            
            SDL_SetRenderDrawColor(scene->renderer,0,0,0,155);
            SDL_RenderFillRect(scene->renderer,NULL);

            if (draw){
                SDL_RenderCopyF(scene->renderer,scene->tryAgain_txt,
                    NULL,&(SDL_FRect){500,450,400,50});
            }

            SDL_RenderCopyF(scene->renderer,scene->gameOver_txt,NULL,&scene->gameOverRect);
        }
    }
    SDL_SetRenderTarget(scene->renderer,NULL);
    SDL_RenderCopy(scene->renderer,scene->wintexture,NULL,NULL);
    SDL_RenderPresent(scene->renderer);
}

int main(){
    emscripten_log(1,"lol2");
    Load();
    srand(time(NULL));
    SDL_Init(SDL_INIT_EVERYTHING);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();
    window=SDL_CreateWindow("Flappy",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        1000,800,
        SDL_WINDOW_SHOWN);
    renderer=SDL_CreateRenderer(window,-1,0);
    font=TTF_OpenFont("assets/Minecraft.ttf",24);
    plr_txt_cache=LoadImage("assets/Player.png",renderer);
    pipe_txt_cache=LoadImage("assets/pipe.png",renderer);
    projectile_txt_cache=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,10,10);
    saw_txt_cache=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,50,50);
    SDL_Texture* prev=SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer,projectile_txt_cache);
    SDL_SetRenderDrawColor(renderer,255,0,0,255);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer,saw_txt_cache);
    SDL_SetRenderDrawColor(renderer,255,255,0,255);
    SDL_RenderFillRect(renderer,NULL);
    SDL_SetRenderTarget(renderer,prev);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    wintexture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,1000,800);
    Scene1 scene;
    init1(&scene, renderer,wintexture);
    currloop=(em_arg_callback_func)loop1;
    emscripten_set_main_loop_arg(currloop, &scene, 0, 1);
}
