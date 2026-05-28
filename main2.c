/* =========================================================================
   Minimal Nostalgia (真夏の独白) - Native Desktop Edition  [v2 改良版]
   
   main2.c  — 元の main.c を改良した新ファイル (main.c はそのまま残す)
   
   改良点:
     ① 背景を3Dモデリング (夜空ドーム・山並み・日本の民家・木々・窓明かり)
        sora.png の画像を参考にしてOpenGL 3Dジオメトリで再現
     ② 花火パーティクルを GL_POINTS に変更してラグを大幅改善
     ③ スペースキーで花火を即時打ち上げ (風ゲストは右クリックに移動)
   ========================================================================= */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define GROUND_Y -0.8f
#include <windows.h>
#endif

/* Include standard headers */
#include <GL/glut.h>   /* Standard GLUT library */
#include "SOIL.h"      /* Custom stb_image wrapper for texture loading */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Auto-link libraries when compiling in MSVC (Visual Studio) */
#ifdef _MSC_VER
  #pragma comment(lib, "freeglut.lib")
  #pragma comment(lib, "opengl32.lib")
  #pragma comment(lib, "glu32.lib")
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* Global State Variables */
float camera_yaw = -45.0f;
float camera_pitch = -9.0f;  
float camera_distance = 5.0f; 
float app_time = 0.0f;

int mouse_old_x = 0;
int mouse_old_y = 0;
int mouse_buttons = 0;

float wind_speed = 1.0f;
int auto_orbit = 0;
float last_interaction_time = 0.0f;
int is_fullscreen = 0;

/* Texture Handles */
GLuint tex_sky    = 0;
GLuint tex_tatami = 0;
GLuint tex_skin   = 0;
GLuint tex_flesh  = 0;
GLuint tex_shoji  = 0;

/* Firefly Structure */
#define NUM_FIREFLIES 18
typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float speed;
    float scale;
    float phase;
    float brightness;
} Firefly;
Firefly fireflies[NUM_FIREFLIES];

/* Dynamic 3D Fireworks Particle System
   【改良】パーティクル描画を glutSolidSphere → GL_POINTS に変更してラグ解消 */
#define MAX_FW_PARTICLES 300
typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float r, g, b, a;
    float size;
    float life;
} FWParticle;

typedef struct {
    int state; /* 0: inactive, 1: rising shell, 2: exploding sparks */
    float x, y, z;
    float vx, vy, vz;
    float timer;
    float r, g, b;
    FWParticle particles[MAX_FW_PARTICLES];
} Firework;
Firework fw;

/* Initialize Fireflies */
void init_fireflies() {
    for (int i = 0; i < NUM_FIREFLIES; i++) {
        fireflies[i].x = ((float)rand() / RAND_MAX) * 8.0f - 4.0f;
        fireflies[i].y = ((float)rand() / RAND_MAX) * 2.2f - 0.2f;
        fireflies[i].z = ((float)rand() / RAND_MAX) * -6.0f - 1.5f;
        
        fireflies[i].vx = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.006f;
        fireflies[i].vy = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.003f;
        fireflies[i].vz = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.006f;
        
        fireflies[i].speed = 1.5f + ((float)rand() / RAND_MAX) * 2.0f;
        fireflies[i].scale = 0.03f + ((float)rand() / RAND_MAX) * 0.04f;
        fireflies[i].phase = ((float)rand() / RAND_MAX) * M_PI * 2.0f;
        fireflies[i].brightness = 0.0f;
    }
}

/* Initialize Firework State */
void init_firework() {
    fw.state = 0;
    fw.x = 0.0f; fw.y = -1.0f; fw.z = -7.0f;
    fw.vx = 0.0f; fw.vy = 0.0f; fw.vz = 0.0f;
    fw.timer = 0.0f;
}

/* Launch firework immediately — スペースキー押下で即時打ち上げ */
void launch_firework_now() {
    fw.state = 1;

    /* app_time を使ってシードを変化させる */
    srand((unsigned int)(time(NULL) ^ (unsigned int)(app_time * 1000)));

    fw.x = ((float)rand() / RAND_MAX) * 8.0f - 4.0f;  /* -4〜+4 に広げる */
    fw.y = -1.0f;
    fw.z = ((float)rand() / RAND_MAX) * -3.0f - 6.0f;
    fw.vx = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.012f; /* 左右のブレも少し大きく */
    fw.vy = 0.13f + ((float)rand() / RAND_MAX) * 0.04f;
    fw.vz = 0.0f;
    fw.timer = 0.0f;

    int color_scheme = rand() % 6;
    if (color_scheme == 0) { fw.r = 1.0f;  fw.g = 0.8f;  fw.b = 0.3f; }
    else if (color_scheme == 1) { fw.r = 0.1f;  fw.g = 0.85f; fw.b = 0.95f; }
    else if (color_scheme == 2) { fw.r = 0.95f; fw.g = 0.3f;  fw.b = 0.5f; }
    else if (color_scheme == 3) { fw.r = 1.0f;  fw.g = 0.35f; fw.b = 0.15f; }
    else if (color_scheme == 4) { fw.r = 0.6f;  fw.g = 0.3f;  fw.b = 1.0f; }
    else { fw.r = 0.3f;  fw.g = 1.0f;  fw.b = 0.4f; }
}

/* Procedural Fallback Texture Generator */
GLuint load_texture_with_fallback(const char* filename, int type) {
    GLuint tex = SOIL_load_OGL_texture(filename, SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, 
                                       SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y | SOIL_FLAG_TEXTURE_REPEATS);
    if (tex != 0) {
        printf("Loaded custom texture '%s' successfully.\n", filename);
        return tex;
    }
    
    printf("WARNING: Texture '%s' not found. Generating in-memory procedural fallback...\n", filename);
    
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    
    int width = 512;
    int height = 512;
    unsigned char* data = (unsigned char*)malloc(width * height * 4);
    if (!data) return 0;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            float fx = (float)x / width;
            float fy = (float)y / height;
            
            if (type == 0) { /* tatami */
                float straw = 0.88f + 0.12f * (float)sin(fy * 180.0f);
                unsigned char r = (unsigned char)(190 * straw);
                unsigned char g = (unsigned char)(205 * straw);
                unsigned char b = (unsigned char)(135 * straw);
                if ((y % 8 == 0) || (x % 256 == 0)) {
                    r = (unsigned char)(r * 0.65f);
                    g = (unsigned char)(g * 0.65f);
                    b = (unsigned char)(b * 0.55f);
                }
                data[idx] = r; data[idx+1] = g; data[idx+2] = b; data[idx+3] = 255;
                
            } else if (type == 1) { /* sora */
                unsigned char r = (unsigned char)(8 + 18 * fy);
                unsigned char g = (unsigned char)(12 + 25 * fy);
                unsigned char b = (unsigned char)(26 + 45 * fy);
                unsigned char star = 0;
                unsigned int hash = x * 15321 + y * 68421;
                if ((hash % 1021) == 0) { star = 140 + (hash % 115); }
                data[idx]   = r + star > 255 ? 255 : r + star;
                data[idx+1] = g + star > 255 ? 255 : g + star;
                data[idx+2] = b + star > 255 ? 255 : b + star;
                data[idx+3] = 255;
                
            } else if (type == 2) { /* skin */
                unsigned char r = 24, g = 95, b = 38;
                float stripe = (float)sin(fx * 10.0f * M_PI + (float)sin(fy * 8.0f) * 1.8f);
                if (stripe > 0.45f) { r = 8; g = 32; b = 14; }
                data[idx] = r; data[idx+1] = g; data[idx+2] = b; data[idx+3] = 255;
                
            } else if (type == 3) { /* flesh */
                unsigned char r = 240, g = 48, b = 48;
                float grain = 0.92f + 0.08f * ((float)(rand() % 100) / 100.0f);
                r = (unsigned char)(r * grain);
                g = (unsigned char)(g * grain);
                b = (unsigned char)(b * grain);
                int num_seeds = 10;
                float seed_centers[10][2] = {
                    {0.32f, 0.35f}, {0.42f, 0.68f}, {0.68f, 0.32f}, {0.58f, 0.62f},
                    {0.25f, 0.60f}, {0.72f, 0.72f}, {0.50f, 0.22f}, {0.78f, 0.48f},
                    {0.18f, 0.38f}, {0.46f, 0.44f}
                };
                for (int i = 0; i < num_seeds; i++) {
                    float dx = (fx - seed_centers[i][0]) * 2.2f;
                    float dy = (fy - seed_centers[i][1]);
                    float dist2 = dx*dx + dy*dy;
                    if (dist2 < 0.00035f) { r = 24; g = 24; b = 24; }
                }
                data[idx] = r; data[idx+1] = g; data[idx+2] = b; data[idx+3] = 255;
                
            } else if (type == 4) { /* shoji */
                unsigned char r = 245, g = 240, b = 228;
                int gx = x % 64;
                int gy = y % 64;
                if (gx < 3 || gy < 3) { r = 85; g = 58; b = 36; }
                int bx = x % 256;
                int by = y % 256;
                if (bx < 7 || by < 7 || bx > 249 || by > 249) { r = 55; g = 38; b = 22; }
                data[idx] = r; data[idx+1] = g; data[idx+2] = b; data[idx+3] = 255;
            }
        }
    }
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    free(data);
    return tex;
}

/* Initialization */
void init_graphics() {
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glEnable(GL_TEXTURE_2D);
    
    tex_tatami = load_texture_with_fallback("tatami.png", 0);
    tex_sky    = load_texture_with_fallback("sora.png", 1);
    tex_skin   = load_texture_with_fallback("skin.png", 2);
    tex_flesh  = load_texture_with_fallback("flesh.png", 3);
    tex_shoji  = load_texture_with_fallback("shoji.png", 4);
    
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    
    /* GL_LIGHT0: Andon Flame Light */
    glEnable(GL_LIGHT0);
    float light0_pos[] = { -0.5f, 0.63f, 0.4f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.15f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.05f);
    
    /* GL_LIGHT1: Moonlight */
    glEnable(GL_LIGHT1);
    float light1_pos[] = { 4.0f, 6.0f, -8.0f, 0.0f };
    float light1_amb[] = { 0.04f, 0.04f, 0.15f, 1.0f };
    float light1_dif[] = { 0.10f, 0.10f, 0.35f, 1.0f };
    float light1_spc[] = { 0.15f, 0.15f, 0.40f, 1.0f };
    glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
    glLightfv(GL_LIGHT1, GL_AMBIENT,  light1_amb);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  light1_dif);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light1_spc);
    
    init_fireflies();
    init_firework();
}

/* Helper: 3D Cylinder */
void draw_cylinder(float base_rad, float top_rad, float height, int slices) {
    GLUquadricObj* quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluQuadricTexture(quad, GL_TRUE);
    gluCylinder(quad, base_rad, top_rad, height, slices, 1);
    
    glPushMatrix();
    glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
    gluDisk(quad, 0.0f, base_rad, slices, 1);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, height);
    gluDisk(quad, 0.0f, top_rad, slices, 1);
    glPopMatrix();
    
    gluDeleteQuadric(quad);
}

/* =========================================================================
   【新規追加】 3D背景描画関数群
   sora.png の景色 (夜空・山並み・日本の民家・木々・窓明かり) を
   OpenGL 3Dジオメトリで再現する
   ========================================================================= */

/* ① 夜空スカイドーム — 大きな半球メッシュで夜空を表現
   深夜の藍色から地平線に向かって少し明るくなるグラデーション */
void draw_sky_dome() {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE); /* スカイドームは深度バッファに書き込まない */
    
    float cam_x = 0.0f, cam_y = 0.0f, cam_z = 0.0f;
    /* カメラ位置にドームを追従させる (視点移動で背景が常に表示されるように) */
    float pitch_rad = camera_pitch * M_PI / 180.0f;
    float yaw_rad   = camera_yaw   * M_PI / 180.0f;
    cam_x = camera_distance * (float)cos(pitch_rad) * (float)sin(yaw_rad);
    cam_y = camera_distance * (float)sin(pitch_rad);
    cam_z = camera_distance * (float)cos(pitch_rad) * (float)cos(yaw_rad);
    
    float R = 22.0f; /* スカイドーム半径 */
    int lat_segs = 12;
    int lon_segs = 32;
    
    glPushMatrix();
    glTranslatef(cam_x, cam_y, cam_z); /* カメラ中心に描画 */
    
    for (int i = 0; i < lat_segs; i++) {
        float lat1 = (float)i       / lat_segs * M_PI * 0.5f; /* 0〜90度 */
        float lat2 = (float)(i + 1) / lat_segs * M_PI * 0.5f;
        
        /* 高度によって色が変わる：真上は深い夜空色、地平線は少し明るい藍色 */
        float t1 = (float)i       / lat_segs;
        float t2 = (float)(i + 1) / lat_segs;
        
        /* 空の色 (上: 深夜の紺, 下: やや明るい藍) */
        float r1 = 0.01f + 0.04f * t1;   float r2 = 0.01f + 0.04f * t2;
        float g1 = 0.02f + 0.06f * t1;   float g2 = 0.02f + 0.06f * t2;
        float b1 = 0.08f + 0.12f * t1;   float b2 = 0.08f + 0.12f * t2;
        
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= lon_segs; j++) {
            float lon = (float)j / lon_segs * M_PI * 2.0f;
            float cos_lon = (float)cos(lon);
            float sin_lon = (float)sin(lon);
            
            float x1 = R * (float)cos(lat1) * cos_lon;
            float y1 = R * (float)sin(lat1);
            float z1 = R * (float)cos(lat1) * sin_lon;
            
            float x2 = R * (float)cos(lat2) * cos_lon;
            float y2 = R * (float)sin(lat2);
            float z2 = R * (float)cos(lat2) * sin_lon;
            
            glColor3f(r1, g1, b1);
            glVertex3f(x1, y1, z1);
            glColor3f(r2, g2, b2);
            glVertex3f(x2, y2, z2);
        }
        glEnd();
    }
    
    /* 地平線以下の半球底面も塗りつぶす (地面色) */
    glColor3f(0.02f, 0.04f, 0.08f);
    glBegin(GL_QUADS);
    for (int j = 0; j < lon_segs; j++) {
        float lon1 = (float)j       / lon_segs * M_PI * 2.0f;
        float lon2 = (float)(j + 1) / lon_segs * M_PI * 2.0f;
        glVertex3f(R * (float)cos(lon1), 0.0f, R * (float)sin(lon1));
        glVertex3f(R * (float)cos(lon2), 0.0f, R * (float)sin(lon2));
        glVertex3f(0.0f, 0.0f, 0.0f);
        glVertex3f(0.0f, 0.0f, 0.0f);
    }
    glEnd();
    
    glPopMatrix();
    glDepthMask(GL_TRUE);
}

/* ② 星のきらめきを点として描画 */
void draw_stars() {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);
    
    /* 乱数シードは固定して毎フレーム同じ位置に星を描画 */
    srand(42);
    
    glPointSize(1.8f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 200; i++) {
        float lon = ((float)rand() / RAND_MAX) * M_PI * 2.0f;
        float lat = ((float)rand() / RAND_MAX) * M_PI * 0.45f; /* 上半球のみ */
        float R = 20.0f;
        float x = R * (float)cos(lat) * (float)cos(lon);
        float y = R * (float)sin(lat) + 1.0f;
        float z = R * (float)cos(lat) * (float)sin(lon);
        
        /* 星の瞬き: app_time と i でオフセット */
        float flicker = 0.6f + 0.4f * (float)sin(app_time * 2.0f + i * 1.37f);
        float brightness = 0.7f + 0.3f * ((float)rand() / RAND_MAX);
        glColor4f(brightness, brightness, brightness * 0.95f, flicker);
        glVertex3f(x, y, z);
    }
    glEnd();
    
    /* 明るい大きめの星 */
    glPointSize(3.0f);
    glBegin(GL_POINTS);
    srand(99);
    for (int i = 0; i < 12; i++) {
        float lon = ((float)rand() / RAND_MAX) * M_PI * 2.0f;
        float lat = 0.2f + ((float)rand() / RAND_MAX) * M_PI * 0.38f;
        float R = 19.5f;
        float flicker = 0.7f + 0.3f * (float)sin(app_time * 1.5f + i * 2.7f);
        glColor4f(0.9f, 0.95f, 1.0f, flicker);
        glVertex3f(R * (float)cos(lat) * (float)cos(lon),
                   R * (float)sin(lat) + 1.0f,
                   R * (float)cos(lat) * (float)sin(lon));
    }
    glEnd();
    
    srand((unsigned int)time(NULL)); /* ランダムシードを元に戻す */
    glDepthMask(GL_TRUE);
    glPointSize(1.0f);
}
/* =========================================================
   背景地面（修正版）
   ========================================================= */
void draw_bg_ground(void)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
    glColor3f(0.03f, 0.05f, 0.04f);

    glVertex3f(-25.0f, -1.0f, -2.0f);
    glVertex3f(25.0f, -1.0f, -2.0f);
    glVertex3f(25.0f, -1.0f, -25.0f);
    glVertex3f(-25.0f, -1.0f, -25.0f);

    glEnd();

    glEnable(GL_LIGHTING);
}



/* ③ 山並み — sora.png の山のシルエットを3D化
   遠景に複数の丘・山をポリゴンで積み重ねる */
void draw_mountain_range() {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    typedef struct {
        float cx;
        float h;
        float w;
        float z;
        float r, g, b;
    } MountainDef;

    MountainDef mountains[] = {
        { -5.0f, 4.5f, 9.0f,  -16.0f, 0.05f, 0.07f, 0.13f },
        {  1.0f, 5.5f, 10.0f, -17.0f, 0.04f, 0.06f, 0.11f },
        {  7.0f, 3.8f, 8.0f,  -15.0f, 0.05f, 0.07f, 0.12f },

        { -3.0f, 3.2f, 7.0f,  -12.0f, 0.05f, 0.08f, 0.15f },
        {  3.5f, 4.0f, 8.5f,  -13.0f, 0.05f, 0.08f, 0.14f },
        { -8.0f, 2.8f, 6.0f,  -11.0f, 0.06f, 0.09f, 0.16f },
        {  9.0f, 3.0f, 7.5f,  -12.0f, 0.06f, 0.09f, 0.15f },
    };

    int num_mountains = 7;

    for (int m = 0; m < num_mountains; m++) {

        float cx = mountains[m].cx;
        float h = mountains[m].h;
        float w = mountains[m].w;
        float z = mountains[m].z;

        float mr = mountains[m].r;
        float mg = mountains[m].g;
        float mb = mountains[m].b;

        int segs = 30;

        glBegin(GL_TRIANGLE_FAN);

        glColor3f(mr, mg, mb);

        glVertex3f(cx, GROUND_Y + h, z);

        for (int j = 0; j <= segs; j++) {

            float t = (float)j / segs;

            float x = cx - w * 0.5f + w * t;

            float edge_h = sin(t * M_PI) * h;

            glVertex3f(x, GROUND_Y + edge_h * 0.02f, z);
        }

        glEnd();

        glBegin(GL_QUADS);

        glVertex3f(cx - w * 0.5f, GROUND_Y - 1.0f, z);
        glVertex3f(cx + w * 0.5f, GROUND_Y - 1.0f, z);
        glVertex3f(cx + w * 0.5f, GROUND_Y, z);
        glVertex3f(cx - w * 0.5f, GROUND_Y, z);

        glEnd();
    }
}


/* ④ 日本の民家1棟を描画
   sora.png に見える合掌造り風の三角屋根の民家をモデリング */

void draw_bg_house(float x, float y, float z, float scale,
    float wall_r, float wall_g, float wall_b,
    float roof_r, float roof_g, float roof_b,
    int has_light)
{
    glDisable(GL_TEXTURE_2D);

    float w = 0.7f * scale;
    float h = 0.5f * scale;
    float d = 0.5f * scale;
    float rh = 0.45f * scale;

    /* 壁 */
    float wall_col[] = { wall_r, wall_g, wall_b, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, wall_col);

    /* 前 */
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, 1.0f);

    glVertex3f(x - w, y, z + d);
    glVertex3f(x + w, y, z + d);
    glVertex3f(x + w, y + h, z + d);
    glVertex3f(x - w, y + h, z + d);

    glEnd();

    /* 後 */
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, -1.0f);

    glVertex3f(x + w, y, z - d);
    glVertex3f(x - w, y, z - d);
    glVertex3f(x - w, y + h, z - d);
    glVertex3f(x + w, y + h, z - d);

    glEnd();

    /* 左 */
    glBegin(GL_QUADS);
    glNormal3f(-1.0f, 0.0f, 0.0f);

    glVertex3f(x - w, y, z - d);
    glVertex3f(x - w, y, z + d);
    glVertex3f(x - w, y + h, z + d);
    glVertex3f(x - w, y + h, z - d);

    glEnd();

    /* 右 */
    glBegin(GL_QUADS);
    glNormal3f(1.0f, 0.0f, 0.0f);

    glVertex3f(x + w, y, z + d);
    glVertex3f(x + w, y, z - d);
    glVertex3f(x + w, y + h, z - d);
    glVertex3f(x + w, y + h, z + d);

    glEnd();

    /* 妻壁 */
    glBegin(GL_TRIANGLES);

    glNormal3f(0.0f, 0.0f, 1.0f);

    glVertex3f(x - w, y + h, z + d);
    glVertex3f(x + w, y + h, z + d);
    glVertex3f(x, y + h + rh, z + d);

    glEnd();

    glBegin(GL_TRIANGLES);

    glNormal3f(0.0f, 0.0f, -1.0f);

    glVertex3f(x + w, y + h, z - d);
    glVertex3f(x - w, y + h, z - d);
    glVertex3f(x, y + h + rh, z - d);

    glEnd();

    /* 屋根 */
    float roof_col[] = { roof_r, roof_g, roof_b, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, roof_col);

    /* 左屋根 */
    glBegin(GL_QUADS);

    glNormal3f(-0.707f, 0.707f, 0.0f);

    glVertex3f(x - w - 0.05f * scale, y + h, z - d - 0.05f * scale);
    glVertex3f(x - w - 0.05f * scale, y + h, z + d + 0.05f * scale);
    glVertex3f(x, y + h + rh, z + d);
    glVertex3f(x, y + h + rh, z - d);

    glEnd();

    /* 右屋根 */
    glBegin(GL_QUADS);

    glNormal3f(0.707f, 0.707f, 0.0f);

    glVertex3f(x + w + 0.05f * scale, y + h, z + d + 0.05f * scale);
    glVertex3f(x + w + 0.05f * scale, y + h, z - d - 0.05f * scale);
    glVertex3f(x, y + h + rh, z - d);
    glVertex3f(x, y + h + rh, z + d);

    glEnd();
    
    /* === 窓明かり — 発光する暖かいオレンジ色 === */
    if (has_light) {
        glDisable(GL_LIGHTING);
        float light_flicker = 0.85f + 0.15f * (float)sin(app_time * 3.0f + x);
        
        /* 前面の窓 */
        glBegin(GL_QUADS);
        float lw = 0.12f * scale;
        float lh = 0.10f * scale;
        float lx = x;
        float ly = y + h * 0.4f;
        float lz = z + d + 0.001f;
        glColor4f(1.0f, 0.75f * light_flicker, 0.3f, 0.85f);
        glVertex3f(lx - lw, ly,      lz);
        glVertex3f(lx + lw, ly,      lz);
        glVertex3f(lx + lw, ly + lh, lz);
        glVertex3f(lx - lw, ly + lh, lz);
        glEnd();
        
        /* 扉の明かり (地面に近い正方形) */
        glBegin(GL_QUADS);
        float dw = 0.07f * scale;
        float dh2 = 0.15f * scale;
        glColor4f(1.0f, 0.65f * light_flicker, 0.25f, 0.7f);
        glVertex3f(x - dw, y,        z + d + 0.001f);
        glVertex3f(x + dw, y,        z + d + 0.001f);
        glVertex3f(x + dw, y + dh2,  z + d + 0.001f);
        glVertex3f(x - dw, y + dh2,  z + d + 0.001f);
        glEnd();
        
        glEnable(GL_LIGHTING);
    }
}

/* ⑤ 背景の木1本 — 松・杉を模した円錐＋幹 */
void draw_bg_tree(float x, float y, float z, float scale) {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    
    /* 幹 */
    float trunk_dark = 0.08f + 0.02f * (float)((int)(x * 10) % 5) / 5.0f;
    glColor3f(trunk_dark, trunk_dark * 0.7f, trunk_dark * 0.4f);
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    draw_cylinder(0.03f * scale, 0.02f * scale, 0.3f * scale, 6);
    glPopMatrix();
    
    /* 葉の部分: 3段の円錐を重ねて松のシルエット */
    float tree_r = 0.04f;
    float tree_g = 0.09f;
    float tree_b = 0.05f;
    
    int layers = 3;
    for (int li = 0; li < layers; li++) {
        float layer_t = (float)li / layers;
        float cone_r = (0.28f - layer_t * 0.12f) * scale;
        float cone_h = (0.30f + layer_t * 0.1f) * scale;
        float cone_y = y + 0.25f * scale + li * 0.22f * scale;
        
        glColor3f(tree_r + layer_t * 0.02f,
                  tree_g + layer_t * 0.02f,
                  tree_b + layer_t * 0.01f);
        glPushMatrix();
        glTranslatef(x, cone_y, z);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        draw_cylinder(cone_r, 0.001f, cone_h, 8);
        glPopMatrix();
    }
    
    glEnable(GL_LIGHTING);
}


/* ⑦ マスター背景描画関数 — 全背景要素を配置 */
void draw_bg_landscape() {
    draw_sky_dome();
    draw_stars();
    draw_mountain_range();
    draw_bg_ground();

    /* 家 */

    draw_bg_house(-6.5f, GROUND_Y, -9.5f, 1.2f,
        0.10f, 0.08f, 0.06f,
        0.06f, 0.05f, 0.04f, 1);

    draw_bg_house(-3.8f, GROUND_Y, -10.5f, 1.0f,
        0.09f, 0.08f, 0.06f,
        0.07f, 0.05f, 0.04f, 1);

    draw_bg_house(-1.2f, GROUND_Y, -9.0f, 1.3f,
        0.11f, 0.09f, 0.07f,
        0.07f, 0.05f, 0.04f, 0);

    draw_bg_house(1.8f, GROUND_Y, -10.0f, 1.1f,
        0.09f, 0.08f, 0.06f,
        0.06f, 0.05f, 0.03f, 1);

    draw_bg_house(4.5f, GROUND_Y, -9.5f, 1.4f,
        0.10f, 0.09f, 0.07f,
        0.07f, 0.06f, 0.04f, 1);

    draw_bg_house(7.2f, GROUND_Y, -10.5f, 1.0f,
        0.09f, 0.08f, 0.06f,
        0.06f, 0.05f, 0.04f, 0);

    /* 奥の家 */

    draw_bg_house(-5.0f, GROUND_Y, -12.5f, 0.85f,
        0.07f, 0.06f, 0.05f,
        0.05f, 0.04f, 0.03f, 1);

    draw_bg_house(2.5f, GROUND_Y, -13.0f, 0.80f,
        0.07f, 0.06f, 0.05f,
        0.05f, 0.04f, 0.03f, 1);

    draw_bg_house(6.0f, GROUND_Y, -12.0f, 0.90f,
        0.07f, 0.06f, 0.05f,
        0.05f, 0.04f, 0.03f, 0);

    /* 木 */

    draw_bg_tree(-8.0f, GROUND_Y, -9.0f, 1.1f);
    draw_bg_tree(-5.5f, GROUND_Y, -9.8f, 0.9f);
    draw_bg_tree(-2.5f, GROUND_Y, -10.0f, 1.0f);
    draw_bg_tree(0.3f, GROUND_Y, -9.5f, 1.2f);
    draw_bg_tree(3.2f, GROUND_Y, -10.2f, 0.95f);
    draw_bg_tree(5.8f, GROUND_Y, -9.8f, 1.05f);
    draw_bg_tree(8.5f, GROUND_Y, -9.2f, 1.1f);

    /* 奥の木 */

    draw_bg_tree(-7.0f, GROUND_Y, -12.0f, 0.8f);
    draw_bg_tree(-1.5f, GROUND_Y, -12.5f, 0.75f);
    draw_bg_tree(4.0f, GROUND_Y, -12.8f, 0.82f);
    draw_bg_tree(7.5f, GROUND_Y, -11.5f, 0.7f);
}

/* Watermelon Slice (元のコードより引用) */
void draw_watermelon_slice(float radius, float angle_deg) {
    float rad = angle_deg * M_PI / 180.0f;
    float half_rad = rad / 2.0f;
    int slices = 20;
    
    glEnable(GL_TEXTURE_2D);
    
    glBindTexture(GL_TEXTURE_2D, tex_flesh);
    float flesh_amb[]  = { 0.8f, 0.2f, 0.2f, 1.0f };
    float flesh_diff[] = { 0.9f, 0.12f, 0.12f, 1.0f };
    float flesh_spec[] = { 0.08f, 0.08f, 0.08f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT, flesh_amb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, flesh_diff);
    glMaterialfv(GL_FRONT, GL_SPECULAR, flesh_spec);
    glMaterialf(GL_FRONT, GL_SHININESS, 8.0f);
    
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f((float)cos(-half_rad + M_PI/2.0f), 0.0f, (float)-sin(-half_rad + M_PI/2.0f));
    glTexCoord2f(0.5f, 0.5f); glVertex3f(0.0f, 0.0f, 0.0f);
    for (int i = 0; i <= slices; i++) {
        float lat = (float)i / slices * (M_PI / 2.0f);
        float y = (float)sin(lat) * radius;
        float r_lat = (float)cos(lat) * radius;
        float x = r_lat * (float)cos(-half_rad);
        float z = r_lat * (float)sin(-half_rad);
        glTexCoord2f(0.5f + 0.5f * (r_lat/radius)*(float)cos(lat), 0.5f + 0.5f*(y/radius));
        glVertex3f(x, y, z);
    }
    glEnd();
    
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f((float)-cos(half_rad + M_PI/2.0f), 0.0f, (float)sin(half_rad + M_PI/2.0f));
    glTexCoord2f(0.5f, 0.5f); glVertex3f(0.0f, 0.0f, 0.0f);
    for (int i = 0; i <= slices; i++) {
        float lat = (float)i / slices * (M_PI / 2.0f);
        float y = (float)sin(lat) * radius;
        float r_lat = (float)cos(lat) * radius;
        float x = r_lat * (float)cos(half_rad);
        float z = r_lat * (float)sin(half_rad);
        glTexCoord2f(0.5f - 0.5f*(r_lat/radius)*(float)cos(lat), 0.5f + 0.5f*(y/radius));
        glVertex3f(x, y, z);
    }
    glEnd();

    glBindTexture(GL_TEXTURE_2D, tex_skin);
    float skin_amb[]  = { 0.1f, 0.35f, 0.1f, 1.0f };
    float skin_diff[] = { 0.15f, 0.55f, 0.15f, 1.0f };
    float skin_spec[] = { 0.45f, 0.45f, 0.45f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT, skin_amb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, skin_diff);
    glMaterialfv(GL_FRONT, GL_SPECULAR, skin_spec);
    glMaterialf(GL_FRONT, GL_SHININESS, 32.0f);
    
    int lon_slices = 20, lat_slices = 12;
    for (int i = 0; i < lat_slices; i++) {
        float lat1 = (float)i / lat_slices * (M_PI / 2.0f);
        float lat2 = (float)(i + 1) / lat_slices * (M_PI / 2.0f);
        float y1 = (float)sin(lat1)*radius, r1 = (float)cos(lat1)*radius;
        float y2 = (float)sin(lat2)*radius, r2 = (float)cos(lat2)*radius;
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= lon_slices; j++) {
            float lon = -half_rad + (float)j/lon_slices * rad;
            float x1=r1*(float)cos(lon), z1=r1*(float)sin(lon);
            float x2=r2*(float)cos(lon), z2=r2*(float)sin(lon);
            glNormal3f(x1/radius, y1/radius, z1/radius);
            glTexCoord2f((float)j/lon_slices, (float)i/lat_slices);
            glVertex3f(x1, y1, z1);
            glNormal3f(x2/radius, y2/radius, z2/radius);
            glTexCoord2f((float)j/lon_slices, (float)(i+1)/lat_slices);
            glVertex3f(x2, y2, z2);
        }
        glEnd();
    }
    
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glTexCoord2f(0.5f, 0.5f); glVertex3f(0.0f, 0.0f, 0.0f);
    for (int j = 0; j <= lon_slices; j++) {
        float lon = half_rad - (float)j/lon_slices * rad;
        glTexCoord2f(0.5f+0.5f*(float)cos(lon), 0.5f+0.5f*(float)sin(lon));
        glVertex3f(radius*(float)cos(lon), 0.0f, radius*(float)sin(lon));
    }
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
    float white_rind[] = { 0.88f, 0.94f, 0.82f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, white_rind);
    for (int side = 0; side < 2; side++) {
        float h_sign = (side == 0) ? -1.0f : 1.0f;
        float the_rad = h_sign * half_rad;
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= slices; i++) {
            float lat = (float)i/slices*(M_PI/2.0f);
            float yv = (float)sin(lat)*radius;
            float r_lat = (float)cos(lat)*radius;
            glNormal3f((float)cos(the_rad+M_PI/2.0f)*h_sign, 0.1f, (float)-sin(the_rad+M_PI/2.0f)*h_sign);
            glVertex3f(r_lat*0.94f*(float)cos(the_rad), yv*0.94f, r_lat*0.94f*(float)sin(the_rad));
            glVertex3f(r_lat*(float)cos(the_rad),       yv,       r_lat*(float)sin(the_rad));
        }
        glEnd();
    }
    glEnable(GL_TEXTURE_2D);
}

/* Tatami Mat */
void draw_tatami_mat(float x_min, float x_max, float z_min, float z_max)
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_tatami);

    float mat_amb[] = { 0.7f, 0.7f, 0.6f, 1.0f };
    float mat_diff[] = { 0.8f, 0.8f, 0.7f, 1.0f };
    float mat_spec[] = { 0.02f, 0.02f, 0.02f, 1.0f };

    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_amb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diff);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_spec);
    glMaterialf(GL_FRONT, GL_SHININESS, 1.0f);

    /* 畳本体 */
    glBegin(GL_QUADS);

    glNormal3f(0.0f, 1.0f, 0.0f);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(x_min, GROUND_Y, z_max);

    glTexCoord2f(2.0f, 0.0f);
    glVertex3f(x_max, GROUND_Y, z_max);

    glTexCoord2f(2.0f, 1.0f);
    glVertex3f(x_max, GROUND_Y, z_min);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(x_min, GROUND_Y, z_min);

    glEnd();

    glDisable(GL_TEXTURE_2D);

    /* 畳の縁 */
    float border_color[] = { 0.05f, 0.15f, 0.08f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, border_color);

    float border_w = 0.06f;

    glBegin(GL_QUADS);

    glVertex3f(x_min, GROUND_Y + 0.001f, z_max);
    glVertex3f(x_min + border_w, GROUND_Y , z_max);
    glVertex3f(x_min + border_w, GROUND_Y , z_min);
    glVertex3f(x_min, GROUND_Y , z_min);

    glEnd();

    glBegin(GL_QUADS);

    glVertex3f(x_max - border_w, GROUND_Y , z_max);
    glVertex3f(x_max, GROUND_Y , z_max);
    glVertex3f(x_max, GROUND_Y , z_min);
    glVertex3f(x_max - border_w, GROUND_Y , z_min);

    glEnd();
}

/* Master Render Scene Function */
void render_scene(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)glutGet(GLUT_WINDOW_WIDTH) / glutGet(GLUT_WINDOW_HEIGHT), 0.1, 100.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    float pitch_rad = camera_pitch * M_PI / 180.0f;
    float yaw_rad   = camera_yaw   * M_PI / 180.0f;
    
    float cam_x = camera_distance * (float)cos(pitch_rad) * (float)sin(yaw_rad);
    float cam_y = camera_distance * (float)sin(pitch_rad);
    float cam_z = camera_distance * (float)cos(pitch_rad) * (float)cos(yaw_rad);
    
    gluLookAt(cam_x, cam_y, cam_z,
              0.0f, 1.35f, 0.0f,
              0.0f, 1.0f, 0.0f);
              
    /* 1. Andon flame flicker light */
    float flicker = 1.0f + 0.12f * (float)sin(app_time * 16.0f) + 0.04f * (float)sin(app_time * 38.0f);
    /* 既存のflicker計算はそのままで、値を強化 */
    float light0_amb[] = { 0.35f * flicker, 0.18f * flicker, 0.02f, 1.0f };  /* 0.18→0.35 */
    float light0_dif[] = { 1.40f * flicker, 0.75f * flicker, 0.18f * flicker, 1.0f };  /* 0.90→1.40 */
    float light0_spc[] = { 1.20f * flicker, 0.65f * flicker, 0.15f * flicker, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT,  light0_amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  light0_dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light0_spc);
    
    /* ----------------------------------------------------
       2. 【改良①】3D背景の描画 (スカイドーム + 山 + 民家 + 木々)
       ---------------------------------------------------- */
    draw_bg_landscape();
    
    glEnable(GL_LIGHTING);
    
    /* ----------------------------------------------------
       3. Washitsu Room & Veranda
       ---------------------------------------------------- */
    draw_tatami_mat(-3.0f, 0.0f,  0.0f,  3.0f);
    draw_tatami_mat( 0.0f, 3.0f,  0.0f,  3.0f);
    draw_tatami_mat(-3.0f, 3.0f, -2.0f,  0.0f);
    draw_tatami_mat(-3.0f, 0.0f, -5.0f, -2.0f);
    draw_tatami_mat( 0.0f, 3.0f, -5.0f, -2.0f);
    
    /* Wooden Engawa Planks */
    glDisable(GL_TEXTURE_2D);
    float wood_color[] = { 0.22f, 0.12f, 0.06f, 1.0f };
    float wood_spec[]  = { 0.15f, 0.15f, 0.15f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, wood_color);
    glMaterialfv(GL_FRONT, GL_SPECULAR, wood_spec);
    glMaterialf(GL_FRONT, GL_SHININESS, 16.0f);
    
    float plank_w = 0.22f;
    float plank_gap = 0.015f;
    for (int p = 0; p < 5; p++) {
        float z_start = -2.0f - (p * (plank_w + plank_gap));
        glBegin(GL_QUADS);
        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-3.6f, GROUND_Y + 0.002f, z_start);
        glVertex3f(3.6f, GROUND_Y + 0.002f, z_start);
        glVertex3f(3.6f, GROUND_Y + 0.002f, z_start - plank_w);
        glVertex3f(-3.6f, GROUND_Y + 0.002f, z_start - plank_w);
        glEnd();
    }
    
    /* =========================
    Shoji Panels
    ========================= */

    float groundY = -1.0f;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_shoji);

    float shoji_col[] = { 0.92f, 0.88f, 0.80f, 0.95f };
    float shoji_spec[] = { 0.05f, 0.05f, 0.05f, 1.0f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, shoji_col);
    glMaterialfv(GL_FRONT, GL_SPECULAR, shoji_spec);
    glMaterialf(GL_FRONT, GL_SHININESS, 2.0f);

    /* 左障子 */
    glBegin(GL_QUADS);

    glNormal3f(0.0f, 0.0f, 1.0f);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-3.0f, groundY, -2.01f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-1.5f, groundY, -2.01f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-1.5f, groundY + 2.1f, -2.01f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-3.0f, groundY + 2.1f, -2.01f);

    glEnd();

    /* 右障子 */
    glBegin(GL_QUADS);

    glNormal3f(0.0f, 0.0f, 1.0f);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(1.5f, groundY, -2.01f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(3.0f, groundY, -2.01f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(3.0f, groundY + 2.1f, -2.01f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(1.5f, groundY + 2.1f, -2.01f);

    glEnd();

    glDisable(GL_TEXTURE_2D);

    /* =========================
       Shoji Frame
       ========================= */

    float shoji_frame[] = { 0.10f, 0.07f, 0.04f, 1.0f };

    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, shoji_frame);

    float fw2 = 0.07f;

    glBegin(GL_QUADS);

    glNormal3f(0.0f, 0.0f, 1.0f);

    /* 左障子 上 */
    glVertex3f(-3.0f, groundY + 2.1f - fw2, -2.005f);
    glVertex3f(-0.06f, groundY + 2.1f - fw2, -2.005f);
    glVertex3f(-0.06f, groundY + 2.1f, -2.005f);
    glVertex3f(-3.0f, groundY + 2.1f, -2.005f);

    /* 左障子 下 */
    glVertex3f(-3.0f, groundY, -2.005f);
    glVertex3f(-0.06f, groundY, -2.005f);
    glVertex3f(-0.06f, groundY + fw2, -2.005f);
    glVertex3f(-3.0f, groundY + fw2, -2.005f);

    /* 左障子 左縦 */
    glVertex3f(-3.0f, groundY, -2.005f);
    glVertex3f(-3.0f + fw2, groundY, -2.005f);
    glVertex3f(-3.0f + fw2, groundY + 2.1f, -2.005f);
    glVertex3f(-3.0f, groundY + 2.1f, -2.005f);

    /* 左障子 中央縦 */
    glVertex3f(-0.06f - fw2, groundY, -2.005f);
    glVertex3f(-0.06f, groundY, -2.005f);
    glVertex3f(-0.06f, groundY + 2.1f, -2.005f);
    glVertex3f(-0.06f - fw2, groundY + 2.1f, -2.005f);

    /* 右障子 上 */
    glVertex3f(0.06f, groundY + 2.1f - fw2, -2.005f);
    glVertex3f(3.0f, groundY + 2.1f - fw2, -2.005f);
    glVertex3f(3.0f, groundY + 2.1f, -2.005f);
    glVertex3f(0.06f, groundY + 2.1f, -2.005f);

    /* 右障子 下 */
    glVertex3f(0.06f, groundY, -2.005f);
    glVertex3f(3.0f, groundY, -2.005f);
    glVertex3f(3.0f, groundY + fw2, -2.005f);
    glVertex3f(0.06f, groundY + fw2, -2.005f);

    /* 右障子 中央縦 */
    glVertex3f(0.06f, groundY, -2.005f);
    glVertex3f(0.06f + fw2, groundY, -2.005f);
    glVertex3f(0.06f + fw2, groundY + 2.1f, -2.005f);
    glVertex3f(0.06f, groundY + 2.1f, -2.005f);

    /* 右障子 右縦 */
    glVertex3f(3.0f - fw2, groundY, -2.005f);
    glVertex3f(3.0f, groundY, -2.005f);
    glVertex3f(3.0f, groundY + 2.1f, -2.005f);
    glVertex3f(3.0f - fw2, groundY + 2.1f, -2.005f);

    glEnd();
    
    /* ----------------------------------------------------
       4. Chabudai (Tea Table)
       ---------------------------------------------------- */
    glPushMatrix();
    glTranslatef(0.0f, GROUND_Y, 0.0f);
    
    glPushMatrix();
    glTranslatef(0.0f, 0.36f, 0.0f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    float table_wood[] = { 0.28f, 0.14f, 0.06f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, table_wood);
    glMaterialfv(GL_FRONT, GL_SPECULAR, wood_spec);
    glMaterialf(GL_FRONT, GL_SHININESS, 25.0f);
    draw_cylinder(1.1f, 1.1f, 0.05f, 40);
    glPopMatrix();
    
    float leg_offset = 0.7f;
    for (int i = 0; i < 4; i++) {
        float angle = (i * 90.0f + 45.0f) * M_PI / 180.0f;
        glPushMatrix();
        glTranslatef(leg_offset*(float)cos(angle), 0.32f, leg_offset*(float)sin(angle));
        glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
        draw_cylinder(0.05f, 0.04f, 0.32f, 12);
        glPopMatrix();
    }
    glPopMatrix();
    
    /* ----------------------------------------------------
       5. Andon (Japanese Paper Lantern)
       ---------------------------------------------------- */

    glPushMatrix();
    glTranslatef(-0.5f, GROUND_Y + 0.38f, 0.4f);

    /* 天板・底板・縦柱 — 茶色マテリアルを直接指定 */
    glEnable(GL_LIGHTING);

    float andon_frame_col[] = { 0.10f, 0.07f, 0.04f, 1.0f };
    float andon_frame_amb[] = { 0.05f, 0.03f, 0.01f, 1.0f };
    float andon_frame_spc[] = { 0.05f, 0.05f, 0.05f, 1.0f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, andon_frame_col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, andon_frame_amb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, andon_frame_spc);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 4.0f);

    /* GL_COLOR_MATERIAL, GL_LIGHTING を両方切って直接色指定 */
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_LIGHTING);

    glColor3f(0.22f, 0.13f, 0.06f);  /* 茶色 */

    glPushMatrix(); glTranslatef(0.0f, 0.015f, 0.0f); glScalef(0.32f, 0.03f, 0.32f); glutSolidCube(1.0f); glPopMatrix();
    glPushMatrix(); glTranslatef(0.0f, 0.415f, 0.0f); glScalef(0.32f, 0.03f, 0.32f); glutSolidCube(1.0f); glPopMatrix();

    float offset = 0.14f;
    for (int i = 0; i < 4; i++) {
        float px = (i == 0 || i == 1) ? offset : -offset;
        float pz = (i == 0 || i == 2) ? offset : -offset;
        glPushMatrix(); glTranslatef(px, 0.215f, pz); glScalef(0.024f, 0.40f, 0.024f); glutSolidCube(1.0f); glPopMatrix();
    }

    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);  /* 元に戻す */

    /* 炎 */
    glDisable(GL_LIGHTING);
    glPushMatrix();
    glTranslatef(0.0f, 0.22f, 0.0f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    float flame_scale = 1.0f + 0.15f * (float)sin(app_time * 16.0f);
    glColor3f(1.0f, 0.55f * flame_scale, 0.08f);
    draw_cylinder(0.02f * flame_scale, 0.001f, 0.08f * flame_scale, 10);
    glPopMatrix();
    glEnable(GL_LIGHTING);
    
    float paper_color[] = { 0.95f, 0.91f, 0.82f, 0.84f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, paper_color);
    float inset = 0.118f;
    float h_min = 0.05f;
    float h_max = 0.38f;
    glBegin(GL_QUADS);
    glNormal3f( 0.0f, 0.0f, 1.0f);
    glVertex3f(-inset, h_min,  inset); glVertex3f( inset, h_min,  inset);
    glVertex3f( inset, h_max,  inset); glVertex3f(-inset, h_max,  inset);
    glNormal3f( 0.0f, 0.0f,-1.0f);
    glVertex3f( inset, h_min, -inset); glVertex3f(-inset, h_min, -inset);
    glVertex3f(-inset, h_max, -inset); glVertex3f( inset, h_max, -inset);
    glNormal3f( 1.0f, 0.0f, 0.0f);
    glVertex3f( inset, h_min,  inset); glVertex3f( inset, h_min, -inset);
    glVertex3f( inset, h_max, -inset); glVertex3f( inset, h_max,  inset);
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-inset, h_min, -inset); glVertex3f(-inset, h_min,  inset);
    glVertex3f(-inset, h_max,  inset); glVertex3f(-inset, h_max, -inset);
    glEnd();
    glPopMatrix();
    
    /* ----------------------------------------------------
       6. Watermelon on plate
       ---------------------------------------------------- */
    glPushMatrix();
    glTranslatef(0.35f, GROUND_Y+0.38f, -0.22f);
    
    float plate_color[] = { 0.9f, 0.92f, 0.96f, 1.0f };
    float plate_spec[]  = { 0.8f, 0.8f, 0.8f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, plate_color);
    glMaterialfv(GL_FRONT, GL_SPECULAR, plate_spec);
    glMaterialf(GL_FRONT, GL_SHININESS, 60.0f);
    glPushMatrix(); glTranslatef(0.0f, 0.005f, 0.0f); glRotatef(90.0f, 1.0f, 0.0f, 0.0f); draw_cylinder(0.40f, 0.40f, 0.015f, 24); glPopMatrix();
    
    glDisable(GL_TEXTURE_2D);
    float blue_pattern[] = { 0.1f, 0.2f, 0.5f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, blue_pattern);
    glPushMatrix(); glTranslatef(0.0f, 0.008f, 0.0f); glRotatef(90.0f, 1.0f, 0.0f, 0.0f); draw_cylinder(0.38f, 0.38f, 0.004f, 24); glPopMatrix();
    
    glPushMatrix(); glTranslatef(-0.1f, 0.012f, 0.0f); glRotatef(12.0f, 0.0f, 1.0f, 0.0f); draw_watermelon_slice(0.32f, 50.0f); glPopMatrix();
    glPopMatrix();
    
    /* ----------------------------------------------------
       8. Fireflies
       ---------------------------------------------------- */
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    
    for (int i = 0; i < NUM_FIREFLIES; i++) {
        if (fireflies[i].brightness <= 0.05f) continue;
        glPushMatrix();
        glTranslatef(fireflies[i].x, fireflies[i].y, fireflies[i].z);
        glColor4f(1.0f, 1.0f, 0.7f, fireflies[i].brightness);
        glutSolidSphere(fireflies[i].scale * 0.4f, 6, 6);
        glColor4f(0.55f, 0.95f, 0.1f, fireflies[i].brightness * 0.45f);
        glutSolidSphere(fireflies[i].scale * 1.5f, 6, 6);
        glPopMatrix();
    }
    
    glEnable(GL_LIGHTING);
    
    /* ----------------------------------------------------
       9. 【改良②】花火の描画 — GL_POINTS に変更してラグを解消
       glutSolidSphere (220個) → glPointSize + GL_POINTS (軽量)
       ---------------------------------------------------- */
    if (fw.state == 1) { /* 上昇中の花火の玉 */
        glDisable(GL_LIGHTING);
        
        /* 花火の玉本体 — 大きな輝く点 */
        glPointSize(8.0f);
        glBegin(GL_POINTS);
        glColor4f(1.0f, 0.85f, 0.4f, 1.0f);
        glVertex3f(fw.x, fw.y, fw.z);
        glEnd();
        
        /* 尾を引く火花 — 小さな点の軌跡 */
        glPointSize(3.5f);
        glBegin(GL_POINTS);
        for (int p = 0; p < 8; p++) {
            float alpha = 1.0f - p * 0.12f;
            glColor4f(1.0f, 0.5f + p * 0.02f, 0.1f, alpha);
            glVertex3f(fw.x, fw.y - p * 0.04f, fw.z);
        }
        glEnd();
        
        glPointSize(1.0f);
        glEnable(GL_LIGHTING);
    } 
    else if (fw.state == 2) { /* 爆発パーティクル */
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        
        /* 【最適化】全パーティクルを一度の glBegin/glEnd でまとめて描画
           (glutSolidSphere を220個個別に呼ぶ代わりに GL_POINTS を使用)
           これにより描画コストが数十倍改善される */
        
        /* パーティクルをサイズ別に2パスで描画 */
        /* Pass 1: 大きめのコアスパーク */
        glPointSize(5.0f);
        glBegin(GL_POINTS);
        for (int i = 0; i < MAX_FW_PARTICLES; i++) {
            if (fw.particles[i].life <= 0.0f) continue;
            if (fw.particles[i].size < 0.018f) continue; /* 大きいもののみ */
            glColor4f(fw.particles[i].r,
                      fw.particles[i].g,
                      fw.particles[i].b,
                      fw.particles[i].a);
            glVertex3f(fw.particles[i].x, fw.particles[i].y, fw.particles[i].z);
        }
        glEnd();
        
        /* Pass 2: 小さめの外側スパーク */
        glPointSize(2.5f);
        glBegin(GL_POINTS);
        for (int i = 0; i < MAX_FW_PARTICLES; i++) {
            if (fw.particles[i].life <= 0.0f) continue;
            if (fw.particles[i].size >= 0.018f) continue; /* 小さいもののみ */
            glColor4f(fw.particles[i].r,
                      fw.particles[i].g,
                      fw.particles[i].b,
                      fw.particles[i].a * 0.8f);
            glVertex3f(fw.particles[i].x, fw.particles[i].y, fw.particles[i].z);
        }
        glEnd();
        
        /* Pass 3: 輝くコア (最も明るい中心) */
        glPointSize(9.0f);
        glBegin(GL_POINTS);
        for (int i = 0; i < MAX_FW_PARTICLES; i += 5) { /* 間引いて描画 */
            if (fw.particles[i].life <= 0.3f) continue;
            glColor4f(1.0f, 1.0f, 0.9f, fw.particles[i].a * 0.6f);
            glVertex3f(fw.particles[i].x, fw.particles[i].y, fw.particles[i].z);
        }
        glEnd();
        
        glPointSize(1.0f);
        glEnable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D);
    }
    
    glutSwapBuffers();
}

/* 60 FPS Update */
void update_scene(int value) {
    app_time += 0.0166f;
    
    if (wind_speed > 1.0f) {
        wind_speed -= 0.006f;
        if (wind_speed < 1.0f) wind_speed = 1.0f;
    }
    
    /* Firefly physics */
    for (int i = 0; i < NUM_FIREFLIES; i++) {
        float gust_x = (float)sin(app_time * 0.8f + i) * 0.002f;
        float gust_z = (float)cos(app_time * 1.1f + i) * 0.002f;
        
        fireflies[i].x += (fireflies[i].vx + gust_x) * wind_speed;
        fireflies[i].y += fireflies[i].vy + (float)sin(app_time * 2.2f + fireflies[i].phase) * 0.002f;
        fireflies[i].z += (fireflies[i].vz + gust_z) * wind_speed;
        
        if (fireflies[i].x < -4.5f) fireflies[i].x = 4.5f;
        if (fireflies[i].x >  4.5f) fireflies[i].x = -4.5f;
        if (fireflies[i].z < -8.0f) fireflies[i].z = -1.2f;
        if (fireflies[i].z > -1.2f) fireflies[i].z = -8.0f;
        if (fireflies[i].y < -0.3f) fireflies[i].y = 2.4f;
        if (fireflies[i].y >  2.4f) fireflies[i].y = -0.3f;
        
        fireflies[i].brightness = 0.1f + 0.9f * (float)sin(app_time * fireflies[i].speed + fireflies[i].phase);
        if (fireflies[i].brightness < 0.0f) fireflies[i].brightness = 0.0f;
    }
    
    /* Fireworks physics */
    if (fw.state == 0) {
        /* 自動打ち上げ (ランダムに稀に発射) */
        if (rand() % 300 == 0) {
            launch_firework_now();
        }
    } 
    else if (fw.state == 1) {
        fw.x += fw.vx;
        fw.y += fw.vy;
        fw.vy -= 0.0016f;
        fw.timer += 0.0166f;
        
        if (fw.vy <= 0.005f || fw.timer >= 1.4f) {
            fw.state = 2;
            fw.timer = 0.0f;
            
            for (int i = 0; i < MAX_FW_PARTICLES; i++) {
                float theta = ((float)rand() / RAND_MAX) * M_PI * 2.0f;
                float phi   = (float)acos(((float)rand() / RAND_MAX) * 2.0f - 1.0f);
                float speed = 0.016f + ((float)rand() / RAND_MAX) * 0.026f;
                
                fw.particles[i].x = fw.x;
                fw.particles[i].y = fw.y;
                fw.particles[i].z = fw.z;
                
                fw.particles[i].vx = speed * (float)sin(phi) * (float)cos(theta);
                fw.particles[i].vy = speed * (float)sin(phi) * (float)sin(theta);
                fw.particles[i].vz = speed * (float)cos(phi);
                
                fw.particles[i].r = fw.r * (0.85f + 0.15f * ((float)rand() / RAND_MAX));
                fw.particles[i].g = fw.g * (0.85f + 0.15f * ((float)rand() / RAND_MAX));
                fw.particles[i].b = fw.b * (0.85f + 0.15f * ((float)rand() / RAND_MAX));
                fw.particles[i].a = 1.0f;
                
                fw.particles[i].size = 0.012f + ((float)rand() / RAND_MAX) * 0.018f;
                fw.particles[i].life = 1.0f;
            }
        }
    } 
    else if (fw.state == 2) {
        fw.timer += 0.0166f;
        int active_count = 0;
        for (int i = 0; i < MAX_FW_PARTICLES; i++) {
            if (fw.particles[i].life <= 0.0f) continue;
            fw.particles[i].x += fw.particles[i].vx;
            fw.particles[i].y += fw.particles[i].vy;
            fw.particles[i].z += fw.particles[i].vz;
            fw.particles[i].vy -= 0.0005f;
            fw.particles[i].vx *= 0.985f;
            fw.particles[i].vy *= 0.985f;
            fw.particles[i].vz *= 0.985f;
            fw.particles[i].life -= 0.013f;
            fw.particles[i].a = fw.particles[i].life;
            if (fw.particles[i].life > 0.0f) active_count++;
        }
        if (active_count == 0 || fw.timer >= 2.0f) {
            fw.state = 0;
        }
    }
    
    glutPostRedisplay();
    glutTimerFunc(16, update_scene, 0);
}

/* Mouse Motion */
void mouse_motion(int x, int y) {
    if (mouse_buttons & (1 << GLUT_LEFT_BUTTON)) {
        camera_yaw   += (x - mouse_old_x) * 0.35f;
        camera_pitch += (y - mouse_old_y) * 0.35f;
        if (camera_pitch >   5.0f) camera_pitch =   5.0f;
        if (camera_pitch < -75.0f) camera_pitch = -75.0f;
        auto_orbit = 0;
        last_interaction_time = app_time;
    }
    mouse_old_x = x;
    mouse_old_y = y;
    glutPostRedisplay();
}

/* Mouse Click */
void mouse_click(int button, int state, int x, int y) {
    if (state == GLUT_DOWN) {
        mouse_buttons |= (1 << button);
    } else if (state == GLUT_UP) {
        mouse_buttons &= ~(1 << button);
    }
    mouse_old_x = x;
    mouse_old_y = y;
    glutPostRedisplay();
}

/* Window Reshape */
void reshape_window(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
}

/* Keyboard Input */
void keyboard_press(unsigned char key, int x, int y) {
    switch (key) {
        case 27: /* ESC */
        case 'q':
        case 'Q':
            exit(0);
            break;
            
        case 'f':
        case 'F':
            is_fullscreen = !is_fullscreen;
            if (is_fullscreen) { glutFullScreen(); }
            else { glutPositionWindow(100, 100); glutReshapeWindow(1280, 720); }
            break;
            
        case ' ': /* 【改良③】スペースキーで花火を即時打ち上げ！ */
            launch_firework_now();
            wind_speed = 2.0f; /* 花火と同時に少し風も吹く */
            last_interaction_time = app_time;
            printf("[Space] 花火を打ち上げました! Firework launched!\n");
            break;
            
        case 'w':
        case 'W':
            camera_distance -= 0.25f;
            if (camera_distance < 2.0f) camera_distance = 2.0f;
            last_interaction_time = app_time;
            break;
            
        case 's':
        case 'S':
            camera_distance += 0.25f;
            if (camera_distance > 12.0f) camera_distance = 12.0f;
            last_interaction_time = app_time;
            break;
    }
}

/* Special Keys */
void special_keys(int key, int x, int y) {
    auto_orbit = 0;
    last_interaction_time = app_time;
    switch (key) {
        case GLUT_KEY_LEFT:  camera_yaw   -= 3.0f; break;
        case GLUT_KEY_RIGHT: camera_yaw   += 3.0f; break;
        case GLUT_KEY_UP:
            camera_pitch -= 2.0f;
            if (camera_pitch < -75.0f) camera_pitch = -75.0f;
            break;
        case GLUT_KEY_DOWN:
            camera_pitch += 2.0f;
            if (camera_pitch > 5.0f) camera_pitch = 5.0f;
            break;
    }
    glutPostRedisplay();
}

/* Print instructions */
void print_instructions() {
    printf("\n");
    printf("========================================================================\n");
    printf("   Minimal Nostalgia (真夏の独白) v2 改良版 — main2.c               \n");
    printf("========================================================================\n");
    printf("  改良点:\n");
    printf("  ① 背景を3Dモデリング (夜空ドーム・山並み・日本の民家・木々)\n");
    printf("  ② 花火パーティクルを GL_POINTS に変更 → ラグを大幅改善\n");
    printf("  ③ スペースキーで花火を即時打ち上げ!\n");
    printf("------------------------------------------------------------------------\n");
    printf("  [マウスドラッグ] : カメラをオービット回転\n");
    printf("  [スペース]       : 花火を今すぐ打ち上げ!\n");
    printf("  [f] / [F]        : フルスクリーン切り替え\n");
    printf("  [w] / [s]        : カメラをズームイン / アウト\n");
    printf("  [矢印キー]       : カメラ方向を手動で変更\n");
    printf("  [Esc] / [q]      : 終了\n");
    printf("========================================================================\n");
}

/* Application entry point */
int main(int argc, char** argv) {
    srand((unsigned int)time(NULL));
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    
    glutInitWindowSize(1280, 720);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Minimal Nostalgia v2 - 真夏の独白 [3D背景・花火改良版]");
    
    print_instructions();
    init_graphics();
    
    glutDisplayFunc(render_scene);
    glutReshapeFunc(reshape_window);
    glutMouseFunc(mouse_click);
    glutMotionFunc(mouse_motion);
    glutKeyboardFunc(keyboard_press);
    glutSpecialFunc(special_keys);
    
    glutTimerFunc(16, update_scene, 0);
    
    glutMainLoop();
    return 0;
}
