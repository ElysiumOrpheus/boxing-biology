#include <raylib.h>
#include <time.h> // For setting random seed
#include <stdio.h> // For loading questions from file
#include <stdlib.h> // exit() in LoadQuestions
#include <math.h> // floor()

#define BOXITO_IMPLEMENTATION
#include "boxito.h"

typedef enum GameState {
    GAMESTATE_MENU,
    GAMESTATE_PLAY,
    GAMESTATE_COUNTDOWN,
    GAMESTATE_DRAW,
    GAMESTATE_END,
    GAMESTATE_CREDITS,
    GAMESTATE_PAUSED,
} GameState;

typedef struct Question {
    char *question;
    int answerCount;
    char **answers;
    int correctAnswer;
    bool usedBefore;
} Question;

Question *questions;
int questionCount;

void LoadQuestions()
{
    FILE *questionFile = fopen("questions.txt", "r");
    char buf[256];
    Question currentQuestion = { 0 };
    bool first = true;

    if (questionFile == NULL)
    {
        perror("Could not load questions file");
        exit(EXIT_FAILURE);
    }

    if (!fgets(buf, 256, questionFile))
    {
        TraceLog(LOG_FATAL, "questions.txt contains no data");
    }

    while (!feof(questionFile))
    {
        switch(buf[0])
        {
            case 'q':
            {
                if (!first)
                {
                    questionCount++;
                    questions = MemRealloc(questions, sizeof(Question) * questionCount);
                    questions[questionCount - 1] = currentQuestion;
                    currentQuestion = (Question) { 0 };
                }
                first = false;

                currentQuestion.question = MemAlloc(512);
                sscanf(buf, "q \"%[^\"\n]\" %d", currentQuestion.question, &(currentQuestion.correctAnswer));
            } break;
            case 'a':
            {
                currentQuestion.answerCount++;
                currentQuestion.answers = MemRealloc(currentQuestion.answers, currentQuestion.answerCount * sizeof(char *));
                currentQuestion.answers[currentQuestion.answerCount - 1] = MemAlloc(256);
                sscanf(buf, "a \"%[^\"\n]\"", currentQuestion.answers[currentQuestion.answerCount - 1]);
            } break;
        }
        if (!fgets(buf, 256, questionFile))
        {
            break;
        }
    }

    questionCount++;
    questions = MemRealloc(questions, sizeof(Question) * questionCount);
    questions[questionCount - 1] = currentQuestion;
    currentQuestion = (Question) { 0 };

    // Randomize question order
    for (int i = 0; i < questionCount; i++)
    {
        Question intermed = questions[i];
        int swapI = GetRandomValue(0, questionCount - 1);
        questions[i] = questions[swapI];
        questions[swapI] = intermed;
    }
}

#define BLOOD_SPLATTER_COUNT 5

typedef struct BloodSplatter {
    int velocityX;
    int velocityY;
    int posX;
    int posY;
} BloodSplatter;

typedef struct Game {
    GameState state;

    int playerTurn; //1 if plr1, 2 if plr2
    float player1Health;
    float player2Health;
    int playerHit;
    int winner;
    float healthTarget;

    Question currentQuestion;
    bool newQuestion;
    int currentQuestionId;
    int questionResultFrameTimer;
    bool answeredQuestion;
    int answerId;
    bool showQuestion;
    int questionFramesLeft;

    BloodSplatter bloodSplatters[BLOOD_SPLATTER_COUNT];
    bool bloodSplattersEnabled;
    bool initBloodSplatter;

    int countDownFrameTimer;
    int globalFrameTimer;
    int drawFrameTimer;
    int punchingFrameTimer;
} Game;

int windowWidth = 1280;
int windowHeight = 720;

bool DrawAnswerButton(const char *answerText, int answerIndex, bool halfsies, bool showResult, bool isCorrectAnswer)
{
    Rectangle rect;
    const int width = (windowWidth / 2 - 70);
    bool ret = false;

    if (answerIndex == 0)
    {
        // Top-left
        rect = (Rectangle) {60, 400, width, halfsies ? 140 : 60};
    }
    else if (answerIndex == 1)
    {
        // Top-right
        rect = (Rectangle) {windowWidth / 2 + 5, 400, width, halfsies ? 140 : 60};
    }
    else if (answerIndex == 2)
    {
        // Bottom-left
        rect = (Rectangle) {60, 470, width, 60};
    }
    else if (answerIndex == 3)
    {
        // Bottom-right
        rect = (Rectangle) {windowWidth / 2 + 5, 470, width, 60};
    }
    else
    {
        TraceLog(LOG_FATAL, "DrawAnswerButton called with invalid answer index (%d)", answerIndex);
    }

    Color color = WHITE;
    Color textColor = BLACK;

    if (showResult)
    {
        color = isCorrectAnswer ? GREEN : RED;
        textColor = WHITE;
    }
    else if (CheckCollisionPointRec(GetMousePosition(), rect))
    {
        textColor = GRAY;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            color = GRAY;
            textColor = WHITE;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
            ret = true;
        }
    }

    DrawRectangleRec((Rectangle) {rect.x - 1, rect.y - 1, rect.width + 2, rect.height + 2}, BLACK);
    DrawRectangleRec(rect, color);
    float size = rect.height;
    if (MeasureText(answerText, size) > rect.width - 10)
    {
        size *= (rect.width - 10) / MeasureText(answerText, size);
    }
    DrawText(answerText, rect.x + rect.width / 2 - MeasureText(answerText, size) / 2, rect.y + rect.height - size, size, textColor);

    return ret;
}

int player1Position = 300;
int player2Position = 700;

Music *jukebox;
int jukeboxCount;
int currentJukeboxId;

void LoadJukebox()
{
    FilePathList files = LoadDirectoryFiles("assets/music");
    jukeboxCount = files.count;

    for (int i = 0; i < files.count; i++)
    {
        char *intermed = files.paths[i];
        int id = GetRandomValue(0, files.count - 1);
        files.paths[i] = files.paths[id];
        files.paths[id] = intermed;
    }

    jukebox = MemAlloc(jukeboxCount * sizeof(Music));

    for (int i = 0; i < jukeboxCount; i++)
    {
        LoadAssetMusic(files.paths[i], &jukebox[i]);
    }
}

Color GetHealthBarColor(float health)
{
    if (health > 0.5f) return GREEN;
    if (health > 0.1f) return YELLOW;
    return RED;
}

#define MIN_TEXT_SIZE 30
#define MAX_TEXT_SIZE 40

typedef enum TextJustification {
    JUSTIFY_LEFT,
    JUSTIFY_RIGHT,
    JUSTIFY_CENTER
} TextJustification;

void DrawTextNL(const char *text, int posX, int posY, int fontSize, Color color, TextJustification justification)
{
    int lineCount;
    const char **lines = TextSplit(text, '\n', &lineCount);
    for (int i = 0; i < lineCount; i++)
    {
        int truePosX;
        if (justification == JUSTIFY_LEFT)
        {
            truePosX = posX;
        }
        else if (justification == JUSTIFY_CENTER)
        {
            //int measure = MeasureText(lines[i], fontSize);
            truePosX = posX - MeasureText(lines[i], fontSize) / 2;
        }
        else if (justification == JUSTIFY_RIGHT)
        {
            TraceLog(LOG_WARNING, "JUSTIFY_RIGHT not supported");
        }
        else
        {
            TraceLog(LOG_WARNING, "DrawTextNL: Invalid justification (%d)", justification);
        }
        DrawText(lines[i], truePosX, posY + fontSize * i + i, fontSize, color);
    }
}

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(windowWidth, windowHeight, "Boxing Science");

    Game game = { 0 };

    Texture2D player1Texture, player2Texture, ringTexture, player1PunchTexture, player2PunchTexture, raylibSticker;
    LoadAssetTexture("assets/boxer_red.png", &player1Texture);
    LoadAssetTexture("assets/boxer_blue.png", &player2Texture);
    LoadAssetTexture("assets/ring.png", &ringTexture);
    LoadAssetTexture("assets/boxer_red_punch.png", &player1PunchTexture);
    LoadAssetTexture("assets/boxer_blue_punch.png", &player2PunchTexture);
    LoadAssetTexture("assets/raylib_128x128.png", &raylibSticker);
    LoadQueuedAssets();

    SetTextureFilter(ringTexture, TEXTURE_FILTER_BILINEAR);
    
    InitAudioDevice();
    Sound bell, correct, incorrect, punch;
    LoadAssetSound("assets/bell.mp3", &bell);
    LoadAssetSound("assets/correct.mp3", &correct);
    LoadAssetSound("assets/incorrect.mp3", &incorrect);
    LoadQueuedAssets();

    Music menu_music, draw_music, win_music;
    LoadAssetMusic("assets/music/main_menu.mp3", &menu_music);
    LoadAssetMusic("assets/draw.mp3", &draw_music);
    LoadAssetMusic("assets/win.mp3", &win_music);
    LoadJukebox();
    LoadQueuedAssets();

    SetRandomSeed(time(NULL));
    LoadQuestions();

    game.state = GAMESTATE_MENU;
    
    const int maxHealth = 20;
    game.playerTurn = 1;
    game.player1Health = maxHealth;
    game.player2Health = maxHealth;
    
    game.newQuestion = true;
    game.showQuestion = true;
    game.questionFramesLeft = 3600;

    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    PlayMusic(menu_music);

    while (!WindowShouldClose())
    {
        if (IsWindowResized())
        {
            windowWidth = GetScreenWidth();
            windowHeight = GetScreenHeight();
            player1Position = windowWidth / 2 - 340;
            player2Position = windowWidth / 2 + 60;
        }

        game.globalFrameTimer++;
        BeginDrawing(); 
        ClearBackground(RAYWHITE);
        DrawTexturePro(ringTexture, (Rectangle){0, 0, ringTexture.width, ringTexture.height}, (Rectangle){0, 0, windowWidth, windowHeight}, (Vector2){0, 0}, 0, WHITE);
        if (game.state == GAMESTATE_MENU)
        {
            DrawTextCentered("Science Boxing", 250, 100, WHITE);
            if (DrawButtonCentered("Play", GREEN, DARKGREEN, DARKGREEN, WHITE, 375))
            {
                game.state = GAMESTATE_COUNTDOWN;
                SetMusicToFadeOut(menu_music);
            }
            if (DrawButtonCentered("Credits", DARKGRAY, GRAY, GRAY, WHITE, 500))
            {
                game.state = GAMESTATE_CREDITS;
            }
        }
        else if (game.state == GAMESTATE_PLAY || game.state == GAMESTATE_COUNTDOWN || game.state == GAMESTATE_PAUSED)
        {
            DrawText("PLAYER 1 HEALTH", player1Position, 570, 20, BLACK);
            DrawRectangle(player1Position, 600, 250, 30, GRAY);
            float health1Progress = (game.player1Health / (float)maxHealth);
            DrawRectangle(player1Position + 5, 605, health1Progress * 240.0f, 20, GetHealthBarColor(health1Progress));

            if (game.state != GAMESTATE_PAUSED && game.playerHit == 1 && game.punchingFrameTimer > 40)
            {
                DrawTexture(player1Texture, player1Position + (game.globalFrameTimer & 1) * 5, 200, WHITE);
                float floored = game.healthTarget;
                game.player1Health -= 0.01;
                if (game.player1Health < floored)
                {
                    game.punchingFrameTimer = 0;
                    game.player1Health = floored;
                    game.playerHit = 0;
                    game.showQuestion = true;
                    game.newQuestion = true;
                    game.playerTurn = 1;
                    game.questionFramesLeft = 3600;
                    game.bloodSplattersEnabled = false;
                    if (game.player1Health == 0)
                    {
                        game.winner = 2;
                        game.state = GAMESTATE_END;
                    }
                }
            }
            else if (game.state != GAMESTATE_PAUSED && game.playerHit == 2 && game.punchingFrameTimer <= 40)
            {
                if (game.punchingFrameTimer == 0) PlaySound(punch);
                game.punchingFrameTimer++;
                DrawTexture(player1PunchTexture, player1Position, 200, WHITE);
            }
            else
            {
                DrawTexture(player1Texture, player1Position, 200, WHITE);
            }

            DrawText("PLAYER 2 HEALTH", player2Position + 250 - MeasureText("PLAYER 2 HEALTH", 20), 570, 20, BLACK);
            DrawRectangle(player2Position, 600, 250, 30, GRAY);
            float health2Progress = (game.player2Health / (float)maxHealth);
            DrawRectangle(player2Position + 5 + (240 - health2Progress * 240.0f), 605, health2Progress * 240.0f, 20, GetHealthBarColor(health2Progress));

            if (game.state != GAMESTATE_PAUSED && game.playerHit == 2 && game.punchingFrameTimer > 40)
            {
                DrawTexture(player2Texture, player2Position + (game.globalFrameTimer & 1) * 5, 200, WHITE);
                float floored = game.healthTarget;
                game.player2Health -= 0.01;
                if (game.player2Health < floored)
                {
                    game.punchingFrameTimer = 0;
                    game.player2Health = floored;
                    game.playerHit = 0;
                    game.showQuestion = true;
                    game.questionFramesLeft = 3600;
                    game.newQuestion = true;
                    game.playerTurn = 2;
                    game.bloodSplattersEnabled = false;
                    if (game.player2Health == 0)
                    {
                        game.winner = 1;
                        game.state = GAMESTATE_END;
                    }
                }
            }
            else if (game.state != GAMESTATE_PAUSED && game.playerHit == 1 && game.punchingFrameTimer <= 40)
            {
                if (game.punchingFrameTimer == 0) PlaySound(punch);
                game.punchingFrameTimer++;
                DrawTexture(player2PunchTexture, player2Position, 200, WHITE);
            }
            else
            {
                DrawTexture(player2Texture, player2Position, 200, WHITE);
            }

            DrawTextCentered(TextFormat("Player %d's turn", game.playerTurn), 100, 40, WHITE);

            if (game.state == GAMESTATE_COUNTDOWN)
            {
                if (game.countDownFrameTimer < 60)
                {
                    DrawTextCentered("3", 250, 100, WHITE);
                }
                else if (game.countDownFrameTimer < 120)
                {
                    DrawTextCentered("2", 250, 100, WHITE);
                }
                else if (game.countDownFrameTimer < 180)
                {
                    DrawTextCentered("1", 250, 100, WHITE);
                }
                else if (game.countDownFrameTimer < 240)
                {
                    PlaySound(bell);
                    if (game.countDownFrameTimer & 0b10) DrawTextCentered("FIGHT!", 250, 100, WHITE);
                }
                else
                {
                    game.state = GAMESTATE_PLAY;
                }
                game.countDownFrameTimer++;
            }
            else if (game.state == GAMESTATE_PLAY)
            {
                if (!IsMusicStreamPlaying(jukebox[currentJukeboxId])) PlayMusic(jukebox[currentJukeboxId]);
                if (GetMusicTimeLength(jukebox[currentJukeboxId]) <= GetMusicTimePlayed(jukebox[currentJukeboxId]))
                {
                    StopMusic(jukebox[currentJukeboxId]);
                    currentJukeboxId++;
                    if (currentJukeboxId == jukeboxCount) currentJukeboxId = 0;
                    PlayMusic(jukebox[currentJukeboxId]);
                }

                if (game.bloodSplattersEnabled && game.globalFrameTimer & 1)
                {
                    

                    if (game.initBloodSplatter)
                    {
                        for (int i = 0; i < BLOOD_SPLATTER_COUNT; i++)
                        {
                            game.bloodSplatters[i] = (BloodSplatter){ GetRandomValue(-5, 5), 0, game.playerHit == 1 ? player1Position + 50 : player2Position + 100, 250};
                        }
                        game.initBloodSplatter = false;
                    }

                    for (int i = 0; i < BLOOD_SPLATTER_COUNT; i++)
                    {
                        game.bloodSplatters[i].velocityY++;
                        game.bloodSplatters[i].posX += game.bloodSplatters[i].velocityX;
                        game.bloodSplatters[i].posY += game.bloodSplatters[i].velocityY;

                        DrawRectangle(game.bloodSplatters[i].posX - 3, game.bloodSplatters[i].posY - 3, 6, 6, (Color){255, 0, 0, 255});
                    }
                    
                }

                if (game.newQuestion)
                {
                    int i;
                    for (i = game.currentQuestionId + 1; i < questionCount; i++)
                    {
                        if (!questions[i].usedBefore) break;
                    }
                    if (i >= questionCount - 1 && questions[i].usedBefore)
                    {
                        for (i = 0; i < game.currentQuestionId; i++)
                        {
                            if (!questions[i].usedBefore) break;
                        }
                        if (i >= game.currentQuestionId - 1 && questions[i].usedBefore)
                        {
                            game.state = GAMESTATE_DRAW;
                        }
                    }
                    game.currentQuestionId = i;
                    game.currentQuestion = questions[game.currentQuestionId];
                    game.newQuestion = false;
                }

                if (game.showQuestion)
                {
                    
// Define drawing constants for easier tweaking
#define TEXT_PADDING 10.0f
#define LINE_SPACING 4.0f

// Draw the UI boxes
DrawRectangle(49, 249, windowWidth - 98, 302, BLACK);
DrawRectangle(50, 250, windowWidth - 100, 300, WHITE);
DrawTextCentered(TextFormat("QUESTION - %0d", game.questionFramesLeft / 60), 250, 50, BLACK);

// --- Text Drawing Logic ---
const char *answerText = game.currentQuestion.question;
Color textColor = BLACK;

// Define the rectangle where the question text should appear.
// NOTE: You might want to make this rectangle larger to use more of the white box.
// For example: Rectangle rect = { 60, 260, windowWidth - 120, 280 };
Rectangle rect = (Rectangle) {50, 300, windowWidth - 98, 70};
float availableWidth = rect.width - (TEXT_PADDING * 2);

// Start by trying to fit the text on one line by adjusting font size
float fontSize = rect.height - (TEXT_PADDING * 2);
if (MeasureText(answerText, fontSize) > availableWidth)
{
    fontSize *= availableWidth / MeasureText(answerText, fontSize);
}

// Clamp font size to your defined limits
if (fontSize > MAX_TEXT_SIZE) fontSize = MAX_TEXT_SIZE;

// If text is still too long even at the minimum font size, we need to wrap it
if (fontSize < MIN_TEXT_SIZE)
{
    fontSize = MIN_TEXT_SIZE;
    
    // --- Multi-line Wrapping Logic ---
    const char *textToProcess = answerText;
    int totalLines = 0;
    
    // First, do a "dry run" to count how many lines we will need
    // This is necessary to calculate the total height for vertical centering
    const char *tempText = textToProcess;
    while (TextLength(tempText) > 0)
    {
        int charsToFit = TextLength(tempText);
        // Find how many characters of the remaining text fit on one line
        while (MeasureText(TextSubtext(tempText, 0, charsToFit), fontSize) > availableWidth && charsToFit > 0)
        {
            charsToFit--;
        }

        // Backtrack to the last space to avoid breaking words
        int lastSpace = -1;
        for (int i = 0; i < charsToFit; i++)
        {
            if (tempText[i] == ' ') lastSpace = i;
        }
        
        // If a space was found and we're not at the end of the text, break there
        if (lastSpace != -1 && (charsToFit < TextLength(tempText)))
        {
            charsToFit = lastSpace;
        }

        totalLines++;
        tempText += charsToFit;
        // Skip leading spaces on the next line
        while (*tempText == ' ') tempText++;
    }

    // Calculate the total height of the text block to center it vertically
    float totalTextHeight = (totalLines * fontSize) + ((totalLines - 1) * LINE_SPACING);
    float currentY = rect.y + (rect.height - totalTextHeight) / 2.0f;

    // Now, do the "real run" to draw each line
    while (TextLength(textToProcess) > 0)
    {
        int charsToFit = TextLength(textToProcess);
        while (MeasureText(TextSubtext(textToProcess, 0, charsToFit), fontSize) > availableWidth && charsToFit > 0)
        {
            charsToFit--;
        }

        int lastSpace = -1;
        for (int i = 0; i < charsToFit; i++)
        {
            if (textToProcess[i] == ' ') lastSpace = i;
        }

        if (lastSpace != -1 && (charsToFit < TextLength(textToProcess)))
        {
            charsToFit = lastSpace;
        }

        // Get the substring for the current line
        const char *line = TextSubtext(textToProcess, 0, charsToFit);
        
        // Calculate X position to center the line horizontally
        float lineX = rect.x + (rect.width - MeasureText(line, fontSize)) / 2.0f;
        
        // Draw the line
        DrawText(line, lineX, currentY, fontSize, textColor);

        // Move to the next line's Y position
        currentY += fontSize + LINE_SPACING;
        
        // Advance the text pointer
        textToProcess += charsToFit;
        while (*textToProcess == ' ') textToProcess++; // Skip leading space
    }
}
else // The text fits on a single line
{
    // Calculate positions for horizontal and vertical centering
    float textWidth = MeasureText(answerText, fontSize);
    float xPos = rect.x + (rect.width - textWidth) / 2.0f;
    float yPos = rect.y + (rect.height - fontSize) / 2.0f;
    
    DrawText(answerText, xPos, yPos, fontSize, textColor);
}


                    bool halfsies = (game.currentQuestion.answerCount == 2);
                    for (int i = 0; i < game.currentQuestion.answerCount; i++)
                    {
                        if (DrawAnswerButton(game.currentQuestion.answers[i], i, halfsies, game.answeredQuestion, i == game.currentQuestion.correctAnswer))
                        {
                            game.answeredQuestion = true;
                            game.answerId = i;
                        }
                    }
                    if (!game.answeredQuestion)
                    {
                        game.questionFramesLeft--;
                        if (game.questionFramesLeft == 0)
                        {
                            game.answeredQuestion = true;
                            game.answerId = -1; // Always get it wrong
                        }
                    }
                }

                if (game.answeredQuestion)
                {
                    if (game.questionResultFrameTimer == 0)
                    {
                        if (game.currentQuestion.correctAnswer == game.answerId)
                        {
                            PlaySound(correct);
                        }
                        else
                        {
                            PlaySound(incorrect);
                        }
                    }
                    game.questionResultFrameTimer++;
                    if (game.questionResultFrameTimer > 120)
                    {
                        game.questionResultFrameTimer = 0;
                        game.answeredQuestion = false;
                        game.showQuestion = false;
                        questions[game.currentQuestionId].usedBefore = true;
                        if (game.currentQuestion.correctAnswer == game.answerId)
                        {
                            // Player got correct answer
                            game.playerHit = (game.playerTurn == 1 ? 2 : 1);
                            game.bloodSplattersEnabled = true;
                            game.initBloodSplatter = true;
                            game.healthTarget = game.playerHit == 1 ? game.player1Health : game.player2Health;
                            game.healthTarget -= (float)game.questionFramesLeft * (3.0f / 3600); // Make it so that the max damage dealt per turn is 3
                        }
                        else
                        {
                            game.questionFramesLeft = 3600;
                            game.showQuestion = true;
                            game.newQuestion = true;
                            game.playerTurn = (game.playerTurn == 1 ? 2 : 1);
                        }
                    }
                }

                if (IsKeyPressed(KEY_ESCAPE)) game.state = GAMESTATE_PAUSED;
            }
            else if (game.state == GAMESTATE_PAUSED)
            {
                DrawRectangle(0, 0, windowWidth, windowHeight, (Color){0, 0, 0, 128});
                DrawTextCentered("Paused", 250, 100, WHITE);
                if (IsKeyPressed(KEY_ESCAPE))
                {
                    game.state = GAMESTATE_PLAY;
                }
                if (DrawButtonCentered("Main Menu", DARKGRAY, GRAY, GRAY, WHITE, 400))
                {
                    game.player1Health = maxHealth;
                    game.player2Health = maxHealth;
                    game.drawFrameTimer = 0;
                    game.countDownFrameTimer = 0;
                    game.state = GAMESTATE_MENU;
                    SetMusicToFadeOut(jukebox[currentJukeboxId]);
                    PlayMusic(menu_music);
                    for (int i = 0; i < questionCount; i++)
                    {
                        questions[i].usedBefore = false;
                    }
                }
                if (DrawButtonCentered("Quit", DARKGRAY, GRAY, GRAY, WHITE, 525))
                {
                    goto quit;
                }
            }
        }
        else if (game.state == GAMESTATE_DRAW)
        {
            if (game.drawFrameTimer == 0) PlayMusic(draw_music);
            game.drawFrameTimer++;
            DrawRectangle(0, 0, windowWidth, windowWidth, (Color){0, 0, 0, game.drawFrameTimer > 255 ? 255 : game.drawFrameTimer});
            DrawTextCentered("It's a draw", 250, 100, WHITE);
            DrawTextCentered("(No more questions)", 360, 30, WHITE);
            if (game.drawFrameTimer > 255)
            {
                if (DrawButtonCentered("Play again", DARKGRAY, GRAY, GRAY, WHITE, 400))
                {
                    game.player1Health = maxHealth;
                    game.player2Health = maxHealth;
                    game.drawFrameTimer = 0;
                    game.countDownFrameTimer = 0;
                    game.state = GAMESTATE_COUNTDOWN;
                    SetMusicToFadeOut(draw_music);
                    for (int i = 0; i < questionCount; i++)
                    {
                        questions[i].usedBefore = false;
                    }
                }
                if (DrawButtonCentered("Quit", DARKGRAY, GRAY, GRAY, WHITE, 525))
                {
                    goto quit;
                }
            }
        }
        else if (game.state == GAMESTATE_END)
        {
            if (game.drawFrameTimer == 0) PlayMusic(win_music);
            game.drawFrameTimer++;
            DrawRectangle(0, 0, windowWidth, windowWidth, (Color){0, 0, 0, game.drawFrameTimer > 255 ? 255 : game.drawFrameTimer});
            DrawTextCentered(TextFormat("Player %d wins", game.winner), 250, 100, WHITE);
            DrawTextCentered("Congratulations!", 360, 30, WHITE);
            if (game.drawFrameTimer > 255)
            {
                if (DrawButtonCentered("Play again", DARKGRAY, GRAY, GRAY, WHITE, 400))
                {
                    game.player1Health = maxHealth;
                    game.player2Health = maxHealth;
                    game.drawFrameTimer = 0;
                    game.countDownFrameTimer = 0;
                    game.state = GAMESTATE_COUNTDOWN;
                    SetMusicToFadeOut(win_music);
                    for (int i = 0; i < questionCount; i++)
                    {
                        questions[i].usedBefore = false;
                    }
                }
                if (DrawButtonCentered("Quit", DARKGRAY, GRAY, GRAY, WHITE, 525))
                {
                    goto quit;
                }
            }
        }
        else if (game.state == GAMESTATE_CREDITS)
        {
            DrawRectangle(0, 0, windowWidth, windowHeight, BLACK);
            int creditStart = 10;
            int headerPadding = 15;
            int bodyPadding = 3;
            int headerSize = 40;
            int bodySize = 25;
            int leadSize = 100;
            DrawTextCentered("Credits", creditStart, leadSize, WHITE);

            int raylibX = 30;
            int raylibY = 70;
            DrawRectangle(raylibX - 2, raylibY - 2, raylibSticker.width + 4, raylibSticker.height + 4, WHITE);
            DrawTexture(raylibSticker, raylibX, raylibY, WHITE);
            DrawText("Made with", raylibX, raylibY - 25, 20, WHITE);

            int designHeader = creditStart + leadSize + headerPadding;
            DrawTextCentered("DESIGN", designHeader, headerSize, WHITE);
            DrawTextCentered("Luke Grant", designHeader + headerSize + bodyPadding, bodySize, WHITE);

            int programmingHeader = designHeader + headerSize + bodyPadding + bodySize + headerPadding;
            DrawTextCentered("PROGRAMMING", programmingHeader, headerSize, WHITE);
            DrawTextCentered("Helix Graziani", programmingHeader + headerSize + bodyPadding, bodySize, WHITE);

            int questionsHeader = programmingHeader + headerSize + bodyPadding + bodySize + headerPadding;
            DrawTextCentered("QUESTIONS", questionsHeader, headerSize, WHITE);
            DrawTextCentered("Owen O'Farrell", questionsHeader + headerSize + bodyPadding, bodySize, WHITE);

            int qaHeader = questionsHeader + headerSize + bodyPadding + bodySize + headerPadding;
            DrawTextCentered("QA", qaHeader, headerSize, WHITE);
            DrawTextCentered("Luke Grant, Owen O'Farrell, Marc Holtzman,", qaHeader + headerSize + bodyPadding, bodySize, WHITE);
            DrawTextCentered("Chris Williams, Antwanne Cardinal, Logan Ricard", qaHeader + headerSize + bodyPadding + bodySize + bodyPadding, bodySize, WHITE);

            int specialThanksHeader = qaHeader + headerSize + bodyPadding + bodySize + bodyPadding + bodySize + headerPadding;
            DrawTextCentered("SPECIAL THANKS", specialThanksHeader, headerSize, WHITE);
            DrawTextCentered("Marc Holtzman", specialThanksHeader + headerSize + bodyPadding, bodySize, WHITE);

            int additions = specialThanksHeader + headerSize + bodyPadding + bodySize + headerPadding;
            DrawTextCentered("Additional credits can be found in assets/CREDITS.txt", additions, bodySize, WHITE);

            if (DrawButtonCentered("Back", DARKGRAY, GRAY, GRAY, WHITE, additions + bodySize + headerPadding))
            {
                game.state = GAMESTATE_MENU;
            }
        }
        DrawText("v1.1.3", 0, 0, 25, WHITE);
        EndDrawing();
        UpdateMusic();
    }
quit:

    for (int i = 0; i < jukeboxCount; i++)
    {
        UnloadMusicStream(jukebox[i]);
    }

    CloseAudioDevice();
    CloseWindow();

    return 0;
}
