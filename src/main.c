#include <raylib.h>
#include <time.h> // For setting random seed
#include <stdio.h> // For loading questions from file
#include <stdlib.h> // exit() in LoadQuestions
#include <math.h> // floor()

typedef enum GameState {
    GAMESTATE_MENU,
    GAMESTATE_PLAY,
    GAMESTATE_COUNTDOWN,
    GAMESTATE_DRAW,
    GAMESTATE_END,
} GameState;

typedef struct Question {
    char *question;
    int answerCount;
    char **answers;
    int correctAnswer;
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

    fgets(buf, 256, questionFile);
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
        fgets(buf, 256, questionFile);
    }

    questionCount++;
    questions = MemRealloc(questions, sizeof(Question) * questionCount);
    questions[questionCount - 1] = currentQuestion;
    currentQuestion = (Question) { 0 };
}

typedef struct Game {
    GameState state;

    int playerTurn; //1 if plr1, 2 if plr2
    float player1Health;
    float player2Health;
    int playerHit;
    int winner;

    Question currentQuestion;
    bool newQuestion;
    int currentQuestionId;
    int questionResultFrameTimer;
    bool answeredQuestion;
    int answerId;
    bool showQuestion;

    int countDownFrameTimer;
    int globalFrameTimer;
    int drawFrameTimer;
    int menuFadeOutFrameTimer;
    int punchingFrameTimer;
} Game;

Texture2D LoadPlayerTexture(const char *path)
{
    return LoadTexture(path);
}

const int windowWidth = 1280;
const int windowHeight = 720;

void DrawTextCentered(const char *text, int posY, int fontSize, Color color)
{
    DrawText(text, windowWidth / 2 - MeasureText(text, fontSize) / 2, posY, fontSize, color);
}

bool DrawAnswerButton(const char *answerText, int answerIndex, bool halfsies, bool showResult, bool isCorrectAnswer)
{
    Rectangle rect;
    const int width = (windowWidth / 2 - 100) - 5;
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

bool DrawButtonCentered(const char *text, Color buttonColor, Color buttonTextColor, Color buttonColorSelected, Color buttonTextColorSelected, int posY)
{
    bool ret = false;
    if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){windowWidth / 2 - 100, posY, 200, 100}))
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
    DrawRectangle(windowWidth / 2 - 100, posY, 200, 100, buttonColor);
    DrawTextCentered(text, posY + 25, 40, buttonTextColor);
    return ret;
}

int main()
{
    InitWindow(windowWidth, windowHeight, "Boxing Science");

    Texture2D player1Texture = LoadPlayerTexture("assets/boxer_red.png");
    Texture2D player2Texture = LoadPlayerTexture("assets/boxer_blue.png");
    Texture2D ringTexture = LoadTexture("assets/ring.png");
    Texture2D player1PunchTexture = LoadPlayerTexture("assets/boxer_red_punch.png");
    Texture2D player2PunchTexture = LoadPlayerTexture("assets/boxer_blue_punch.png");

    InitAudioDevice();
    Sound bell = LoadSound("assets/bell.mp3");
    Sound correct = LoadSound("assets/correct.mp3");
    Sound incorrect = LoadSound("assets/incorrect.mp3");
    Sound win = LoadSound("assets/win.mp3");
    Sound punch = LoadSound("assets/punch.mp3");

    Music menu_music = LoadMusicStream("assets/music/main_menu.mp3");
    PlayMusicStream(menu_music);
    Music draw_music = LoadMusicStream("assets/music/draw.mp3");

    SetRandomSeed(time(NULL));
    LoadQuestions();

    Game game = { 0 };
    game.state = GAMESTATE_MENU;
    
    const int maxHealth = 20;
    game.playerTurn = 1;
    game.player1Health = maxHealth;
    game.player2Health = maxHealth;
    
    game.newQuestion = true;
    game.showQuestion = true;

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        game.globalFrameTimer++;
        BeginDrawing(); 
        ClearBackground(RAYWHITE);
        DrawTexturePro(ringTexture, (Rectangle){0, 0, ringTexture.width, ringTexture.height}, (Rectangle){0, 0, windowWidth, windowHeight}, (Vector2){0, 0}, 0, WHITE);
        if (game.state == GAMESTATE_MENU)
        {
            UpdateMusicStream(menu_music);
            DrawTextCentered("Science Boxing", 250, 100, WHITE);
            if (DrawButtonCentered("Play", GREEN, DARKGREEN, DARKGREEN, WHITE, 375))
            {
                game.state = GAMESTATE_COUNTDOWN;
                game.menuFadeOutFrameTimer = 100;
            }
        }
        else if (game.state == GAMESTATE_PLAY || game.state == GAMESTATE_COUNTDOWN)
        {
            if (game.menuFadeOutFrameTimer)
            {
                if (IsMusicStreamPlaying(menu_music))
                {
                    SetMusicVolume(menu_music, (float)game.menuFadeOutFrameTimer / 100);
                    UpdateMusicStream(menu_music);
                }
                if (IsMusicStreamPlaying(draw_music))
                {
                    SetMusicVolume(draw_music, (float)game.menuFadeOutFrameTimer / 100);
                    UpdateMusicStream(draw_music);
                }
                if (IsSoundPlaying(win))
                {
                    SetSoundVolume(win, (float)game.menuFadeOutFrameTimer / 100);
                }
                game.menuFadeOutFrameTimer--;
            }
            DrawText("PLAYER 1 HEALTH", 200, 570, 20, BLACK);
            DrawRectangle(200, 600, 250, 30, GRAY);
            DrawRectangle(205, 605, (game.player1Health / (float)maxHealth) * 240.0f, 20, GREEN);

            if (game.playerHit == 1 && game.punchingFrameTimer > 40)
            {
                DrawTexture(player1Texture, 200 + (game.globalFrameTimer & 1) * 5, 200, WHITE);
                float floored = floorf(game.player1Health);
                game.player1Health -= 0.01;
                if (game.player1Health < floored)
                {
                    game.punchingFrameTimer = 0;
                    game.player1Health = floored;
                    game.playerHit = 0;
                    game.showQuestion = true;
                    game.newQuestion = true;
                    game.playerTurn = 1;
                    if (game.player1Health == 0)
                    {
                        game.winner = 2;
                        game.state = GAMESTATE_END;
                    }
                }
            }
            else if (game.playerHit == 2 && game.punchingFrameTimer <= 40)
            {
                if (game.punchingFrameTimer == 0) PlaySound(punch);
                game.punchingFrameTimer++;
                DrawTexture(player1PunchTexture, 200, 200, WHITE);
            }
            else
            {
                DrawTexture(player1Texture, 200, 200, WHITE);
            }

            DrawText("PLAYER 2 HEALTH", 1050 - MeasureText("PLAYER 2 HEALTH", 20), 570, 20, BLACK);
            DrawRectangle(800, 600, 250, 30, GRAY);
            DrawRectangle(805, 605, (game.player2Health / (float)maxHealth) * 240.0f, 20, GREEN);

            if (game.playerHit == 2 && game.punchingFrameTimer > 40)
            {
                DrawTexture(player2Texture, 800 + (game.globalFrameTimer & 1) * 5, 200, WHITE);
                float floored = floorf(game.player2Health);
                game.player2Health -= 0.01;
                if (game.player2Health < floored)
                {
                    game.punchingFrameTimer = 0;
                    game.player2Health = floored;
                    game.playerHit = 0;
                    game.showQuestion = true;
                    game.newQuestion = true;
                    game.playerTurn = 2;
                    if (game.player2Health == 0)
                    {
                        game.winner = 1;
                        game.state = GAMESTATE_END;
                    }
                }
            }
            else if (game.playerHit == 1 && game.punchingFrameTimer <= 40)
            {
                if (game.punchingFrameTimer == 0) PlaySound(punch);
                game.punchingFrameTimer++;
                DrawTexture(player2PunchTexture, 800, 200, WHITE);
            }
            else
            {
                DrawTexture(player2Texture, 800, 200, WHITE);
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
                if (game.newQuestion)
                {
                    if (game.currentQuestionId == questionCount)
                    {
                        game.state = GAMESTATE_DRAW;
                        continue;
                    }
                    game.currentQuestion = questions[game.currentQuestionId];
                    game.currentQuestionId++;
                    game.newQuestion = false;
                }

                if (game.showQuestion)
                {
                    DrawRectangle(49, 249, windowWidth - 98, 302, BLACK);
                    DrawRectangle(50, 250, windowWidth - 100, 300, WHITE);
                    DrawTextCentered("QUESTION:", 250, 50, BLACK);
                    const char *answerText = game.currentQuestion.question;
                    Color textColor = BLACK;
                    Rectangle rect = (Rectangle) {50, 300, windowWidth - 100, 70};
                    float size = rect.height;
                    if (MeasureText(answerText, size) > rect.width - 10)
                    {
                        size *= (rect.width - 10) / MeasureText(answerText, size);
                    }
                    DrawText(answerText, rect.x + rect.width / 2 - MeasureText(answerText, size) / 2, rect.y + rect.height - size, size, textColor);

                    bool halfsies = (game.currentQuestion.answerCount == 2);
                    for (int i = 0; i < game.currentQuestion.answerCount; i++)
                    {
                        if (DrawAnswerButton(game.currentQuestion.answers[i], i, halfsies, game.answeredQuestion, i == game.currentQuestion.correctAnswer))
                        {
                            game.answeredQuestion = true;
                            game.answerId = i;
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
                        if (game.currentQuestion.correctAnswer == game.answerId)
                        {
                            // Player got correct answer
                            game.playerHit = (game.playerTurn == 1 ? 2 : 1);
                            if (game.playerHit == 1)
                            {
                                game.player1Health -= 0.01;
                            }
                            else
                            {
                                game.player2Health -= 0.01;
                            }
                        }
                        else
                        {
                            game.showQuestion = true;
                            game.playerTurn = (game.playerTurn == 1 ? 2 : 1);
                        }
                    }
                }
            }
        }
        else if (game.state == GAMESTATE_DRAW)
        {
            if (game.drawFrameTimer == 0) PlayMusicStream(draw_music);
            UpdateMusicStream(draw_music);
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
                    game.menuFadeOutFrameTimer = 50;
                }
                if (DrawButtonCentered("Quit", DARKGRAY, GRAY, GRAY, WHITE, 525))
                {
                    CloseWindow();
                    return 0;
                }
            }
        }
        else if (game.state == GAMESTATE_END)
        {
            if (game.drawFrameTimer == 0) PlaySound(win);
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
                    game.menuFadeOutFrameTimer = 50;
                }
                if (DrawButtonCentered("Quit", DARKGRAY, GRAY, GRAY, WHITE, 525))
                {
                    CloseWindow();
                    return 0;
                }
            }
        }
        EndDrawing();
    }

    return 0;
}
