#ifndef SMOOTH_ANIMATION_H
#define SMOOTH_ANIMATION_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
    typedef enum
    {
        SMOOTH_SPRING_FAST,
        SMOOTH_SPRING_BOUNCE,
        SMOOTH_MENU,
        SMOOTH_EASING,
        SMOOTH_SEQUENCE,
        SMOOTH_SCENE_COUNT
    } SmoothScene;
    typedef struct
    {
        SmoothScene scene;
        uint32_t scene_ms;
        uint32_t stage;
        float target_x, target_y, current_x, current_y, width, height;
        float lanes[4];
    } SmoothSnapshot;
    /* Explicit initialization after hardware/time setup; one foreground caller. */
    void SmoothAnimation_Init(uint64_t now_us);
    void SmoothAnimation_Update(uint64_t now_us);
    void SmoothAnimation_SelectScene(SmoothScene scene, uint64_t now_us);
    void SmoothAnimation_Stop(void);
    void SmoothAnimation_Snapshot(SmoothSnapshot *snapshot);
    const char *SmoothAnimation_SceneName(SmoothScene scene);
#ifdef __cplusplus
}
#endif
#endif
