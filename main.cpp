#include <mbed.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <time.h>

#include <mpr121.h>
#include <PinDetect.h>

#include <SongPlayer.h>

#include "uLCD_4DGL.h"

using namespace std;

DigitalOut led1(LED1);
DigitalOut led2(LED2);
DigitalOut led3(LED3);
DigitalOut led4(LED4);

PinDetect pb(p8);

SongPlayer speaker(p21);

// Create the interrupt receiver object on pin 26
InterruptIn interrupt(p26);

uLCD_4DGL uLCD(p28, p27, p30);

// Setup the Serial to the PC for debugging
Serial pc(USBTX, USBRX);

// Setup the i2c bus on pins 28 and 27
I2C i2c(p9, p10);

// Setup the Mpr121:
// constructor(i2c object, i2c address of the mpr121)
Mpr121 mpr121(&i2c, Mpr121::ADD_VSS);

int currentButton = 0;
int last1Button = 0;
int last2Button = 0;
int last3Button = 0;

int controlMode = 0; //0 = swipe, 1 = press

int pressed = 0; //0 = no press, 1 = press

bool combo;

AnalogIn analog (p20);
 
unsigned int random_generator (void)
{
    unsigned int x = 0;
    unsigned int iRandom = 0;

    for (x = 0; x <= 32; x += 2)
    {
        iRandom += ((analog.read_u16() % 3) << x);
        wait_us (10);
    }
    
    return iRandom;
}

// Creates a new 2048 board
int** CreateNewBoard()
{
    int** board = 0;
    board = new int*[4];

    for (int row = 0; row < 4; row++)
    {
        board[row] = new int[4];
        for (int col = 0; col < 4; col++)
        {
            board[row][col] = 0;
        }
    }

    return board;
}

// Updates the squares on the uLCD display
void UpdateBoard(int** board, map<int, int> color_map)
{
    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            int value = board[col][row];
            char buffer[50];
            sprintf(buffer, "%d", value);
            uLCD.filled_rectangle(32*row+1, 32*col+1, 32*row+30, 32*col+30, color_map[value]);
            uLCD.text_mode(TRANSPARENT);
            uLCD.text_bold(ON);
            uLCD.text_string(buffer, 4.5*row + 1.5, 4*col + 1.5, FONT_7X8, BLACK);
        }
    }
}

// Gets all empty squares in the 2048 board
vector< vector<int> > GetEmptySquares(int** board)
{
    vector< vector<int> > emptySquares;

    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            if (board[row][col] == 0)
            {
                vector<int> emptySquare;
                emptySquare.push_back(row);
                emptySquare.push_back(col);
                emptySquares.push_back(emptySquare);
            }
        }
    }

    return emptySquares;
}

// Adds a random square of value 2 to the board
void AddRandomSquare(int** board)
{
    vector< vector<int> > emptySquares = GetEmptySquares(board);

    int randSquare = random_generator() % emptySquares.size();
    vector<int> square = emptySquares[randSquare];

    int numAdd = random_generator() % 4;
    if (numAdd == 0) {
        board[square.at(0)][square.at(1)] = 4;
    } else {
        board[square.at(0)][square.at(1)] = 2;
    }
}

// Determines if the board is completely full
bool IsBoardFull(int** board)
{
    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            if (board[row][col] == 0)
            {
                return false;
            }
        }
    }

    return true;
}

// Checks if the player has any valid moves left
bool BoardHasMoves(int** board)
{
    if (!IsBoardFull(board))
    {
        return true;
    }

    // Check for valid SwipeRights
    for (int row = 0; row < 4; row++)
    {
        for (int col = 3; col > 0; col--)
        {
            if (board[row][col] == board[row][col - 1])
            {
                return true;
            }
        }
    }

    // Check for valid SwipeLefts
    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 3; col++)
        {
            if (board[row][col] == board[row][col + 1])
            {
                return true;
            }
        }
    }

    // Check for valid SwipeUps
    for (int col = 0; col < 4; col++)
    {
        for (int row = 0; row < 3; row++)
        {
            if (board[row][col] == board[row + 1][col])
            {
                return true;
            }
        }
    }

    // Check for valid SwipeDowns
    for (int col = 0; col < 4; col++)
    {
        for (int row = 3; row > 0; row--)
        {
            if (board[row][col] == board[row - 1][col])
            {
                return true;
            }
        }
    }

    // No valid swipes, game over
    return false;
}

// Player swipes right
void SwipeRight(int** board)
{
    // Work right to left
    for (int row = 0; row < 4; row++)
    {
        // Shift squares (repeat 3x to make sure all shifts happen)
        for (int i = 0; i < 3; i++)
        {
            for (int col = 3; col > 0; col--)
            {
                if (board[row][col] == 0)
                {
                    board[row][col] = board[row][col - 1];
                    board[row][col - 1] = 0;
                }
            }
        }

        // Merge squares
        for (int col = 3; col > 0; col--)
        {
            if (board[row][col] == board[row][col - 1])
            {
                combo = true;
                board[row][col] *= 2;
                board[row][col - 1] = 0;
            }
        }

        // Shift one more time after merge
        for (int col = 3; col > 0; col--)
        {
            if (board[row][col] == 0)
            {
                board[row][col] = board[row][col - 1];
                board[row][col - 1] = 0;
            }
        }
    }
}

// Player swipes left
void SwipeLeft(int** board)
{
    // Work left to right
    for (int row = 0; row < 4; row++)
    {
        // Shift squares (repeat 3x to make sure all shifts happen)
        for (int i = 0; i < 3; i++)
        {
            for (int col = 0; col < 3; col++)
            {
                if (board[row][col] == 0)
                {
                    board[row][col] = board[row][col + 1];
                    board[row][col + 1] = 0;
                }
            }
        }

        // Merge squares
        for (int col = 0; col < 3; col++)
        {
            if (board[row][col] == board[row][col + 1])
            {
                combo = true;
                board[row][col] *= 2;
                board[row][col + 1] = 0;
            }
        }

        // Shift one more time after merge
        for (int col = 0; col < 3; col++)
        {
            if (board[row][col] == 0)
            {
                board[row][col] = board[row][col + 1];
                board[row][col + 1] = 0;
            }
        }
    }
}

// Player swipes up
void SwipeUp(int** board)
{
    // Work top to bottom
    for (int col = 0; col < 4; col++)
    {
        // Shift squares (repeat 3x to make sure all shifts happen)
        for (int i = 0; i < 3; i++)
        {
            for (int row = 0; row < 3; row++)
            {
                if (board[row][col] == 0)
                {
                    board[row][col] = board[row + 1][col];
                    board[row + 1][col] = 0;
                }
            }
        }

        // Merge squares
        for (int row = 0; row < 3; row++)
        {
            if (board[row][col] == board[row + 1][col])
            {
                combo = true;
                board[row][col] *= 2;
                board[row + 1][col] = 0;
            }
        }

        // Shift one more time after merge
        for (int row = 0; row < 3; row++)
        {
            if (board[row][col] == 0)
            {
                board[row][col] = board[row + 1][col];
                board[row + 1][col] = 0;
            }
        }
    }
}

// Player swipes down
void SwipeDown(int** board)
{
    // Work bottom to top
    for (int col = 0; col < 4; col++)
    {
        // Shift squares (repeat 3x to make sure all shifts happen)
        for (int i = 0; i < 3; i++)
        {
            for (int row = 3; row > 0; row--)
            {
                if (board[row][col] == 0)
                {
                    board[row][col] = board[row - 1][col];
                    board[row - 1][col] = 0;
                }
            }
        }

        // Merge squares
        for (int row = 3; row > 0; row--)
        {
            if (board[row][col] == board[row - 1][col])
            {
                combo = true;
                board[row][col] *= 2;
                board[row - 1][col] = 0;
            }
        }

        // Shift one more time after merging
        for (int row = 3; row > 0; row--)
        {
            if (board[row][col] == 0)
            {
                board[row][col] = board[row - 1][col];
                board[row - 1][col] = 0;
            }
        }
    }
}



void fallInterrupt() {
    int numberPad=0;
    int i=0;
    int value=mpr121.read(0x00);
    value +=mpr121.read(0x01)<<8;
    i=0;
    for (i=0; i<12; i++) {
        if (((value>>i)&0x01)==1) numberPad=i+1;
    }
    if (numberPad != 0) {
        if (controlMode == 0) {
            if ( numberPad != currentButton) {
                last3Button = last2Button;
                last2Button = last1Button;
                last1Button = currentButton;
                currentButton = numberPad;
            }
        } else if (controlMode == 1) {
            last3Button = 0;
            last2Button = 0;
            last1Button = 0;
            currentButton = numberPad;
        }
    }
    pressed = 1;
}


void pb_hit_callback (void) {
    controlMode++;
    controlMode = controlMode % 2;
}

int main() {
    
    srand(time(NULL));

    int** board = CreateNewBoard();

    map<int, int> color_map;
    color_map[0] = 0x000000;
    color_map[2] = 0xFF0000;
    color_map[4] = 0xFF8000;
    color_map[8] = 0xFFFF00;
    color_map[16] = 0x00FF00;
    color_map[32] = 0x00FF80;
    color_map[64] = 0x00FFFF;
    color_map[128] = 0x0008FF;
    color_map[256] = 0x0000FF;
    color_map[512] = 0x8000FF;
    color_map[1024] = 0xFF00FF;
    color_map[2048] = 0xFF0080;

    AddRandomSquare(board);
    AddRandomSquare(board);
    AddRandomSquare(board);
    AddRandomSquare(board);
    AddRandomSquare(board);
 
    interrupt.fall(&fallInterrupt);
    interrupt.mode(PullUp);
    pb.mode(PullUp);
    wait(0.1);
    pb.attach_deasserted(&pb_hit_callback);
    pb.setSampleFrequency();
    int direction = 0; //0 = idle, 1 = up, 2 = right, 3 = down, 4 = left
    float duration[2] = {0.5, 0.0};
    float up[2] = {1200, 0};
    float right[2] =  {1300, 0};
    float down[2] = {1400, 0};
    float left[2] = {1500, 0};
    uLCD.background_color(0x000000);
    uLCD.rectangle(0, 0, 127, 127, 0xFFFFFF);
    uLCD.baudrate(3000000);
    uLCD.line(0, 31, 127, 31, 0xFFFFFF);
    uLCD.line(0, 32, 127, 32, 0xFFFFFF);
    uLCD.line(0, 63, 127, 63, 0xFFFFFF);
    uLCD.line(0, 64, 127, 64, 0xFFFFFF);
    uLCD.line(0, 95, 127, 95, 0xFFFFFF);
    uLCD.line(0, 96, 127, 96, 0xFFFFFF);
    uLCD.line(31, 0, 31, 127, 0xFFFFFF);
    uLCD.line(32, 0, 32, 127, 0xFFFFFF);
    uLCD.line(63, 0, 63, 127, 0xFFFFFF);
    uLCD.line(64, 0, 64, 127, 0xFFFFFF);
    uLCD.line(95, 0, 95, 127, 0xFFFFFF);
    uLCD.line(96, 0, 96, 127, 0xFFFFFF);

    UpdateBoard(board, color_map);

    while (1) {

        combo = false;

        if (!BoardHasMoves(board))
        {
            uLCD.filled_rectangle(0, 0, 127, 127, 0x000000);
            uLCD.text_mode(TRANSPARENT);
            uLCD.text_bold(ON);
            char buffer2[50]= "GAME OVER";
            uLCD.text_string(buffer2, 6, 6, FONT_7X8, WHITE);
            break;
        }
        

        switch (controlMode) {
            case 0:
                if (currentButton == 8 && last1Button == 7 && last2Button == 6 && last3Button == 5) {
                    direction = 1;
                } else if (currentButton == 11 && last1Button == 7 && last2Button == 3) {
                    direction = 2;
                } else if (currentButton == 5 && last1Button == 6 && last2Button == 7 && last3Button == 8) {
                    direction = 3;
                } else if (currentButton == 3 && last1Button == 7 && last2Button == 11) {
                    direction = 4;
                } else {
                    direction = 0;
                }
                break;
            case 1:
                if (currentButton == 8) {
                    direction = 1;
                } else if (currentButton == 11) {
                    direction = 2;
                } else if (currentButton == 5) {
                    direction = 3;
                } else if (currentButton == 3) {
                    direction = 4;
                } else {
                    direction = 0;
                }
                break;
        }
        if (pressed == 0) {
            direction = 0;
        }
        switch (direction) {
            case 0:
                break;
            case 1:
                SwipeDown(board);
                if (combo) 
                {
                    speaker.PlaySong(down, duration);
                }
                UpdateBoard(board, color_map);
                wait(0.2);
                AddRandomSquare(board);
                UpdateBoard(board, color_map);
                break;
            case 2:
                SwipeLeft(board);
                if (combo) 
                {
                    speaker.PlaySong(left, duration);
                }
                UpdateBoard(board, color_map);
                wait(0.2);
                AddRandomSquare(board);
                UpdateBoard(board, color_map);
                break;
            case 3:
                SwipeUp(board);
                if (combo) 
                {
                    speaker.PlaySong(up, duration);
                }
                UpdateBoard(board, color_map);
                wait(0.2);
                AddRandomSquare(board);
                UpdateBoard(board, color_map);
                break;
            case 4:
                SwipeRight(board);
                if (combo) 
                {
                    speaker.PlaySong(right, duration);
                }
                UpdateBoard(board, color_map);
                wait(0.2);
                AddRandomSquare(board);
                UpdateBoard(board, color_map);
                break;
    }

    pc.printf("%d  %d  %d  %d  \n\rControl Mode: %d \n\rDirection: %d\n\rPressed: %d\n\r", currentButton, last1Button, last2Button, last3Button, controlMode, direction, pressed);
    pressed = 0;
    wait(0.5);

    }
}



