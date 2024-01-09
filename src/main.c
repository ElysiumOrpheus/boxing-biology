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

int main()
{
    InitWindow(windowWidth, windowHeight, "Boxing Science");

    Texture2D player1Texture = LoadPlayerTexture("assets/boxer_red.png");
    Texture2D player2Texture = LoadPlayerTexture("assets/boxer_blue.png");

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
        if (game.state == GAMESTATE_MENU)
        {
            DrawTextCentered("Science Boxing", 250, 100, BLACK);
            Color buttonColor = GREEN;
            Color buttonTextColor = DARKGREEN;
            if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){windowWidth / 2 - 100, 375, 200, 100}))
            {
                buttonTextColor = WHITE;
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
                {
                    buttonColor = DARKGREEN;
                }
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                {
                    game.state = GAMESTATE_COUNTDOWN;
                }
            }
            DrawRectangle(windowWidth / 2 - 100, 375, 200, 100, buttonColor);
            DrawTextCentered("Play", 400, 40, buttonTextColor);
        }
        else if (game.state == GAMESTATE_PLAY || game.state == GAMESTATE_COUNTDOWN)
        {
            DrawText("PLAYER 1 HEALTH", 200, 570, 20, BLACK);
            DrawRectangle(200, 600, 250, 30, GRAY);
            DrawRectangle(205, 605, (game.player1Health / (float)maxHealth) * 240.0f, 20, GREEN);

            if (game.playerHit == 1)
            {
                DrawTexture(player1Texture, 200 + (game.globalFrameTimer & 1) * 5, 200, WHITE);
                float floored = floorf(game.player1Health);
                game.player1Health -= 0.01;
                if (game.player1Health < floored)
                {
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
            else
            {
                DrawTexture(player1Texture, 200, 200, WHITE);
            }

            DrawText("PLAYER 2 HEALTH", 1050 - MeasureText("PLAYER 2 HEALTH", 20), 570, 20, BLACK);
            DrawRectangle(800, 600, 250, 30, GRAY);
            DrawRectangle(805, 605, (game.player2Health / (float)maxHealth) * 240.0f, 20, GREEN);

            if (game.playerHit == 2)
            {
                DrawTexture(player2Texture, 800 + (game.globalFrameTimer & 1) * 5, 200, WHITE);
                float floored = floorf(game.player2Health);
                game.player2Health -= 0.01;
                if (game.player2Health < floored)
                {
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
            else
            {
                DrawTexture(player2Texture, 800, 200, WHITE);
            }

            DrawTextCentered(TextFormat("Player %d's turn", game.playerTurn), 100, 40, BLACK);

            if (game.state == GAMESTATE_COUNTDOWN)
            {
                if (game.countDownFrameTimer < 60)
                {
                    DrawTextCentered("3", 250, 100, BLACK);
                }
                else if (game.countDownFrameTimer < 120)
                {
                    DrawTextCentered("2", 250, 100, BLACK);
                }
                else if (game.countDownFrameTimer < 180)
                {
                    DrawTextCentered("1", 250, 100, BLACK);
                }
                else if (game.countDownFrameTimer < 240)
                {
                    if (game.countDownFrameTimer & 1) DrawTextCentered("FIGHT!", 250, 100, BLACK);
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
                    DrawTextCentered(game.currentQuestion.question, 310, 70, BLACK);

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
            game.drawFrameTimer++;
            DrawRectangle(0, 0, windowWidth, windowWidth, (Color){0, 0, 0, game.drawFrameTimer > 255 ? 255 : game.drawFrameTimer});
            DrawTextCentered("It's a draw", 250, 100, WHITE);
            DrawTextCentered("(No more questions)", 360, 30, WHITE);
        }
        else if (game.state == GAMESTATE_END)
        {
            game.drawFrameTimer++;
            DrawRectangle(0, 0, windowWidth, windowWidth, (Color){0, 0, 0, game.drawFrameTimer > 255 ? 255 : game.drawFrameTimer});
            DrawTextCentered(TextFormat("Player %d wins", game.winner), 250, 100, WHITE);
            DrawTextCentered("Congratulations!", 360, 30, WHITE);
        }
        EndDrawing();
    }

    return 0;
}
