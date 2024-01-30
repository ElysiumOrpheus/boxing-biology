/*******************************************************************************************
*
*   boxito v1.0 - An engine to create 2D video games
*
*   FEATURES:
*       - Loading screen
*
*   DEPENDENCIES:
*
*   LICENSE: Proprietary License
*
*   Copyright (c) 2024 Helix Graziani. All Rights Reserved.
*
*   Unauthorized copying of this file, via any medium is strictly prohibited
*   This project is proprietary and confidential unless the owner allows
*   usage in any other form by expressly written permission.
*
**********************************************************************************************/


#ifndef BOXITO_H
#define BOXITO_H

#include "raylib.h"

// Function specifiers in case library is build/used as a shared library (Windows)
// NOTE: Microsoft specifiers to tell compiler that symbols are imported/exported from a .dll
#if defined(_WIN32)
    #if defined(BUILD_LIBTYPE_SHARED)
        #define BOXITOAPI __declspec(dllexport)     // We are building the library as a Win32 shared library (.dll)
    #elif defined(USE_LIBTYPE_SHARED)
        #define BOXITOAPI __declspec(dllimport)     // We are using the library as a Win32 shared library (.dll)
    #endif
#endif

// Function specifiers definition
#ifndef BOXITOAPI
    #define BOXITOAPI       // Functions defined as 'extern' by default (implicit specifiers)
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
// Allow custom memory allocators
#ifndef BOXITO_MALLOC
    #define BOXITO_MALLOC(sz)       malloc(sz)
#endif
#ifndef BOXITO_CALLOC
    #define BOXITO_CALLOC(n,sz)     calloc(n,sz)
#endif
#ifndef BOXITO_FREE
    #define BOXITO_FREE(p)          free(p)
#endif


//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" { // Prevents name mangling of functions
#endif

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
BOXITOAPI void DrawTextCentered(const char *text, int posY, int fontSize, Color color);
BOXITOAPI bool DrawButtonCentered(const char *text, Color buttonColor, Color buttonTextColor, Color buttonColorSelected, Color buttonTextColorSelected, int posY);
BOXITOAPI void UpdateLoadingScreen(int loadingMax, int loadingProgress);

BOXITOAPI void LoadAssetTexture(const char *filename, Texture2D *out);
BOXITOAPI void LoadAssetMusic(const char *filename, Music *out);
BOXITOAPI void LoadAssetSound(const char *filename, Sound *out);
BOXITOAPI void LoadQueuedAssets(void);

#ifdef __cplusplus
}
#endif

#endif // BOXITO_H

#ifdef BOXITO_IMPLEMENTATION

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#define windowHeight GetScreenHeight()
#define windowWidth GetScreenWidth()

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

typedef enum {
    ASSET_TEXTURE,
    ASSET_MUSIC,
    ASSET_SOUND,
} AssetType;

typedef struct AssetLoadEntry {
    void *out;
    const char *filename;
    AssetType type;
} AssetLoadEntry;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
AssetLoadEntry *loadEntries;
int loadEntryCount;

//----------------------------------------------------------------------------------
// Module specific Functions Declaration
//----------------------------------------------------------------------------------
// ...

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
void DrawTextCentered(const char *text, int posY, int fontSize, Color color)
{
    DrawText(text, windowWidth / 2 - MeasureText(text, fontSize) / 2, posY, fontSize, color);
}

bool DrawButtonCentered(const char *text, Color buttonColor, Color buttonTextColor, Color buttonColorSelected, Color buttonTextColorSelected, int posY)
{
    bool ret = false;
    int width = MeasureText(text, 40);
    if (width < 190) width = 190;
    width += 10;
    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){windowWidth / 2 - width / 2, posY, width, 100}))
    {
        buttonTextColor = buttonTextColorSelected;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            buttonColor = buttonColorSelected;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
            ret = true;
        }
    }
    DrawRectangle(windowWidth / 2 - width / 2, posY, width, 100, buttonColor);
    DrawTextCentered(text, posY + 25, 40, buttonTextColor);
    return ret;
}

void UpdateLoadingScreen(int loadingMax, int loadingProgress)
{
    if (WindowShouldClose()) exit(EXIT_SUCCESS);
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTextCentered("Loading...", 250, 100, WHITE);
    DrawTextCentered(TextFormat("%d / %d", loadingProgress, loadingMax), 360, 30, WHITE);
    DrawRectangle(windowWidth / 2 - 400, 400, 800, 50, GRAY);
    DrawRectangle(windowWidth / 2 - 400, 400, ((float)loadingProgress / loadingMax) * 800, 50, WHITE);
    EndDrawing();
}

void LoadAssetTexture(const char *filename, Texture2D *out)
{
    loadEntryCount++;
    loadEntries = MemRealloc(loadEntries, loadEntryCount * sizeof(AssetLoadEntry));
    loadEntries[loadEntryCount - 1] = (AssetLoadEntry){out, filename, ASSET_TEXTURE};
}

void LoadAssetMusic(const char *filename, Music *out)
{
    loadEntryCount++;
    loadEntries = MemRealloc(loadEntries, loadEntryCount * sizeof(AssetLoadEntry));
    loadEntries[loadEntryCount - 1] = (AssetLoadEntry){out, filename, ASSET_MUSIC};
}

void LoadAssetSound(const char *filename, Sound *out)
{
    loadEntryCount++;
    loadEntries = MemRealloc(loadEntries, loadEntryCount * sizeof(AssetLoadEntry));
    loadEntries[loadEntryCount - 1] = (AssetLoadEntry){out, filename, ASSET_SOUND};
}

void LoadQueuedAssets(void)
{
    int loadingProgress = 0;
    int loadingMax = loadEntryCount;

    for (int i = 0; i < loadEntryCount; i++)
    {
        loadingProgress++;
        switch (loadEntries[i].type)
        {
            case ASSET_TEXTURE:
            {
                *(Texture2D*)loadEntries[i].out = LoadTexture(loadEntries[i].filename);
            } break;
            case ASSET_MUSIC:
            {
                *(Music*)loadEntries[i].out = LoadMusicStream(loadEntries[i].filename);
            } break;
            case ASSET_SOUND:
            {
                *(Sound*)loadEntries[i].out = LoadSound(loadEntries[i].filename);
            } break;
        }
        UpdateLoadingScreen(loadingMax, loadingProgress);
    }

    loadEntryCount = 0;
    MemFree(loadEntries);
    loadEntries = NULL;
}

#undef windowHeight
#undef windowWidth

#endif // BOXITO_IMPLEMENTATION

