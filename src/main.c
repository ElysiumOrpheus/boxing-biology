#include <raylib.h>
#include <time.h> // For setting random seed
#include <stdio.h> // For loading questions from file
#include <stdlib.h> // exit() in LoadQuestions

typedef enum GameState {
    GAMESTATE_MENU,
    GAMESTATE_PLAY
} GameState;

typedef struct Question {
    char *question;
    int answerCount;
    char **answers;
    char *answerIds;
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

                currentQuestion.question = MemAlloc(512);
                sscanf("q \"%s\" %c", currentQuestion.question, &(currentQuestion.correctAnswer));
            } break;
            case 'a':
            {
                currentQuestion.answerCount++;
                currentQuestion.answerIds = MemRealloc(currentQuestion.answerIds, currentQuestion.answerCount);
                currentQuestion.answers = MemRealloc(currentQuestion.answers, currentQuestion.answerCount * sizeof(char *));
                currentQuestion.answers[currentQuestion.answerCount - 1] = MemAlloc(256);
                sscanf("a %c \"%s\"", &(currentQuestion.answerIds[currentQuestion.answerCount - 1]), currentQuestion.answers[currentQuestion.answerCount - 1]);
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
    int player1Health;
    int player2Health;

    Question currentQuestion;
} Game;

int main()
{
    const int windowWidth = 1280;
    const int windowHeight = 720;
    InitWindow(windowWidth, windowHeight, "Boxing Science");

    SetRandomSeed(time(NULL));
    LoadQuestions();

    Game game = { 0 };
    game.state = GAMESTATE_MENU;
    
    const int maxHealth = 20;
    game.playerTurn = 1;
    game.player1Health = maxHealth;
    game.player2Health = maxHealth;

reroll:
    int player1Roll = GetRandomValue(1, 6);
    int player2Roll = GetRandomValue(1, 6);

    if (player1Roll > player2Roll)
    {
        game.playerTurn = 1;
    }
    else if (player2Roll > player1Roll)
    {
        game.playerTurn = 2;
    }
    else
    {
        goto reroll;
    }

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing(); 
        ClearBackground(RAYWHITE);
        if (game.state == GAMESTATE_MENU)
        {
            DrawText("Science Boxing", windowWidth / 2 - MeasureText("Science Boxing", 100) / 2, 250, 100, BLACK);
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
                    game.state = GAMESTATE_PLAY;
                }
            }
            DrawRectangle(windowWidth / 2 - 100, 375, 200, 100, buttonColor);
            DrawText("Play", windowWidth / 2 - MeasureText("Play", 40) / 2, 400, 40, buttonTextColor);
        }
        else if (game.state == GAMESTATE_PLAY)
        {
            // Draw the health bars
            DrawText("PLAYER 1 HEALTH", 200, 570, 20, BLACK);
            DrawRectangle(200, 600, 250, 30, GRAY);
            DrawRectangle(205, 605, ((float)game.player1Health / maxHealth) * 240, 20, GREEN);

            DrawText("PLAYER 2 HEALTH", 800, 570, 20, BLACK);
            DrawRectangle(800, 600, 250, 30, GRAY);
            DrawRectangle(805, 605, ((float)game.player1Health / maxHealth) * 240, 20, GREEN);

            const char *turnText = TextFormat("Player %d's turn", game.playerTurn);
            DrawText(turnText, windowWidth / 2 - MeasureText(turnText, 40) / 2, 100, 40, BLACK);
        }
        EndDrawing();
    }

    return 0;
}
