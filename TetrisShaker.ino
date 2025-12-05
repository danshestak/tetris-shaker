#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <BLEGamepadClient.h>

class Grid {
private:
  int** arr;
public:
  int width;
  int height;

  // 1. Standard Constructor
  Grid(int x, int y) {
    init(x, y);
  }

  // 2. Helper for initialization
  void init(int x, int y) {
    this->width = x;
    this->height = y;
    arr = new int*[y];
    for (int i = 0; i < height; ++i) {
      arr[i] = new int[width];
      for (int j = 0; j < width; ++j) {
        arr[i][j] = 0;
      }
    }
  }

  // 3. COPY CONSTRUCTOR (The Fix!)
  // This performs a "Deep Copy" so the data survives after initialization
  Grid(const Grid& other) {
    this->width = other.width;
    this->height = other.height;
    
    // Allocate NEW memory
    arr = new int*[height];
    for (int i = 0; i < height; ++i) {
      arr[i] = new int[width];
      for (int j = 0; j < width; ++j) {
        // Copy the values from the other grid
        arr[i][j] = other.arr[i][j];
      }
    }
  }

  // 4. Assignment Operator (Good practice to prevent memory leaks on = )
  Grid& operator=(const Grid& other) {
    if (this == &other) return *this; // Handle self-assignment
    
    // Delete old data
    for (int i = 0; i < height; ++i) delete[] arr[i];
    delete[] arr;

    // Allocate and copy new data
    this->width = other.width;
    this->height = other.height;
    arr = new int*[height];
    for (int i = 0; i < height; ++i) {
      arr[i] = new int[width];
      for (int j = 0; j < width; ++j) {
        arr[i][j] = other.arr[i][j];
      }
    }
    return *this;
  }

  int get(int x, int y, int rotations) const {
    int R = (rotations % 4 + 4) % 4; 
    int x_prime, y_prime;
    const int W = width;
    const int H = height;

    switch (R) {
      case 0: x_prime = x; y_prime = y; break;
      case 1: x_prime = y; y_prime = W - 1 - x; break;
      case 2: x_prime = W - 1 - x; y_prime = H - 1 - y; break;
      case 3: x_prime = H - 1 - y; y_prime = x; break;
      default: return -1; 
    }

    // Bounds check on the INTERNAL array indices
    if (x_prime < 0 || x_prime >= W || y_prime < 0 || y_prime >= H) {
        return -1; 
    }

    return arr[y_prime][x_prime];
  }

  Grid& set(int x, int y, int value) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
       arr[y][x] = value;
    }
    return *this;
  }

  ~Grid() {
    for (int i = 0; i < height; ++i) {
        delete[] arr[i];
    }
    delete[] arr;
  }
};

constexpr int rendererValueToColor[8][3] = {
  {0, 0, 0},
  {255, 0, 0},
  {255, 127, 0},
  {255, 255, 0},
  {0, 255, 0},
  {0, 255, 255},
  {0, 0, 255},
  {191, 0, 255}
};
const Grid shapeGrids[] = {
  Grid(3, 3) //Z
  .set(0, 0, 1)
  .set(1, 0, 1)
  .set(1, 1, 1)
  .set(2, 1, 1), 
  Grid(3, 3) //L
  .set(2, 0, 2)
  .set(0, 1, 2)
  .set(1, 1, 2)
  .set(2, 1, 2), 
  Grid(2, 2) //O
  .set(0, 0, 3)
  .set(1, 0, 3)
  .set(0, 1, 3)
  .set(1, 1, 3), 
  Grid(3, 3) //S
  .set(1, 0, 4)
  .set(2, 0, 4)
  .set(0, 1, 4)
  .set(1, 1, 4), 
  Grid(4, 4) //I
  .set(0, 1, 5)
  .set(1, 1, 5)
  .set(2, 1, 5)
  .set(3, 1, 5), 
  Grid(3, 3) //J
  .set(0, 0, 6)
  .set(0, 1, 6)
  .set(1, 1, 6)
  .set(2, 1, 6), 
  Grid(3, 3) //T
  .set(1, 0, 7)
  .set(0, 1, 7)
  .set(1, 1, 7)
  .set(2, 1, 7)
};
constexpr char shapes[7] = {'Z', 'L', 'O', 'S', 'I', 'J', 'T'};
constexpr int srsKicks[4][5][2] = {
  // 0 -> R 
  {{0,0}, {-1,0}, {-1,+1}, {0,-2}, {-1,-2}},
  // R -> 2 
  {{0,0}, {+1,0}, {+1,-1}, {0,+2}, {+1,+2}},
  // 2 -> L 
  {{0,0}, {+1,0}, {+1,+1}, {0,-2}, {+1,-2}},
  // L -> 0 
  {{0,0}, {-1,0}, {-1,-1}, {0,+2}, {-1,+2}},
};
constexpr int srsIKicks[4][5][2] = {
  // 0 -> R 
  {{0,0}, {-2,0}, {+1,0}, {-2,+1}, {+1,-2}},
  // R -> 2 
  {{0,0}, {-1,0}, {+2,0}, {-1,-2}, {+2,+1}},
  // 2 -> L 
  {{0,0}, {+2,0}, {-1,0}, {+2,-1}, {-1,+2}},
  // L -> 0 
  {{0,0}, {+1,0}, {-2,0}, {+1,+2}, {-2,-1}},
};

Adafruit_NeoPixel matrix = Adafruit_NeoPixel(256, 15, NEO_GRB + NEO_KHZ800);
class Renderer {
private:
  static int coordToIndex(int x, int y) {
    if (x >= 16 || y >= 16) { //TESTING
      return 0;
    }

    if (y%2 == 0) {
      return y*16 + x;
    } else {
      return y*16 + (15-x);
    }
  }
  
public:
  Renderer() {
    matrix.begin();
    matrix.setBrightness(2);
    matrix.clear();
    matrix.show();
  }

  void renderPixel(int x, int y, int value) {
    matrix.setPixelColor(
      this->coordToIndex(x, y), 
      rendererValueToColor[value][0], 
      rendererValueToColor[value][1], 
      rendererValueToColor[value][2]
    );
  }
};

class Board {
private:
  int score;
  int level;
  int linesCleared;
  Grid* grid;
public:
  Board() {
    this->grid = new Grid(10, 16);
    this->score = 0;
    this->level = 1;
    this->linesCleared = 0;
  }

  int getWidth() {
    return this->grid->width;
  }

  int getHeight() {
    return this->grid->height;
  }

  int get(int x, int y) {
    return this->grid->get(x, y, 0);
  }

  void set(int x, int y, int value) {
    this->grid->set(x, y, value);
  }

  void clearFullLines(bool tSpin) {
    int newLinesCleared = 0;
    const int W = this->getWidth();
    const int H = this->getHeight();

    int y_write = H - 1; 

    for (int y_read = H - 1; y_read >= 0; y_read--) {
      bool lineFull = true;

      for (int x = 0; x < W; x++) {
        if (this->get(x, y_read) == 0) {
          lineFull = false;
          break;
        }
      }

      if (!lineFull) {
        if (y_read != y_write) {
          for (int x = 0; x < W; x++) {
            this->set(x, y_write, this->get(x, y_read));
          }
        }
        y_write--;
      } else {
        newLinesCleared++;
      }
    }
    
    for (int y = 0; y < newLinesCleared; y++) {
      for (int x = 0; x < W; x++) {
        this->set(x, y, 0);
      }
    }

    int newScore = 0;
    if (newLinesCleared > 0) {
      if (!tSpin) {
        switch (newLinesCleared) {
          case 1:
            newScore = this->level*100;
            break;
          case 2:
            newScore = this->level*300;
            break;
          case 3:
            newScore = this->level*500;
            break;
          case 4:
            newScore = this->level*800;
            break;
        }
      } else {
        switch (newLinesCleared) {
          case 1:
            newScore = this->level*800;
            break;
          case 2:
            newScore = this->level*1200;
            break;
          case 3:
            newScore = this->level*1600;
            break;
        }
      }

      this->linesCleared += newLinesCleared;
      this->score += newScore;
    }
  }

  void awardPoints(int newPoints) {
    this->score += newPoints;
  }

  void render(Renderer& renderer) {
    for (int x = 0; x < this->getWidth(); x++) {
      for (int y = 0; y < this->getHeight(); y++) {
        renderer.renderPixel(x, y, this->get(x, y));
      }
    }
  }

  ~Board() {
    delete grid;
  }
};

class Piece {
private:
  int posX;
  int posY;
  int rot;
  char bag[14];
  char hold;
  bool holdAvailable;
  bool grounded;
  bool performedTSpin;
  unsigned long nextFall;
  unsigned long lastGrounded;

  void newBag(int bagNumber) { //there are two bags to support a next piece ui
    for (int i = 7*(bagNumber-1); i < 7*bagNumber; i++) {
      this->bag[i] = shapes[i%7];
    }

    for (int i = 7*bagNumber - 1; i > 7*(bagNumber-1); i--) {
      int j = random(7*(bagNumber-1), i+1);

      const char temp = this->bag[i];
      this->bag[i] = this->bag[j];
      this->bag[j] = temp;
    }
  }

  void resetPos() {
    this->posX = this->bag[0] == 'O' ? 4 : 3;
    this->posY = 0;
    this->rot = 0;
    this->performedTSpin = false;
  }

  bool tSpinEligible(Board& board) {
    if (this->getBagChar(0) != 'T') {
      return false;
    }

    int corners = 0;
    for (int x = 0; x < 2; x++) {
      for (int y = 0; y < 2; y++) {
        if (board.get(this->posX + 2*x, this->posY + 2*y) != 0) {
          corners++;
        }
      }
    }
    return corners >= 3;
  }
  
public:
  static const Grid& getShapeGridByChar(char c) {
    for (int i = 0; i < 7; i++) {
      if (shapes[i] == c) {
        return shapeGrids[i];
      }
    }
    return shapeGrids[2];
  }

  Piece() {
    this->newBag(1);
    this->newBag(2);
    this->resetPos();
    this->hold = '_';
    this->holdAvailable = true;
    this->resetFallTimer(2000);
    this->grounded = false;
    this->lastGrounded = 0;
  }

  int getPosX() {
    return this->posX;
  }

  int getPosY() {
    return this->posY;
  }

  int getRot() {
    return this->rot;
  }

  char getBagChar(int i) {
    return this->bag[i];
  }

  bool swapHold() {
    if (!this->holdAvailable) {
      return false;
    }

    if (this->hold == '_') {
      this->hold = this->getBagChar(0);
      this->next();
    } else {
      char temp = this->getBagChar(0);
      this->bag[0] = this->hold;
      this->hold = temp;
    }

    this->holdAvailable = false;
    return true;
  }

  void next() {
    for (int i = 0; i < 13; i++) {
      this->bag[i] = this->bag[i+1];
    }

    this->bag[13] = '_';

    if (this->getBagChar(7) == '_') {
      this->newBag(2);
    }

    this->resetPos();
    this->resetFallTimer(2000);
  }

  void resetFallTimer(int delta) {
    this->nextFall = millis() + delta;
  }

  bool collidesWithBoard(Board& board) {
    const Grid& shape = this->getShapeGridByChar(this->getBagChar(0));
    
    for (int relX = 0; relX < shape.width; relX++) {
      for (int relY = 0; relY < shape.height; relY++) {
        if (shape.get(relX, relY, this->getRot()) == 0) {
          continue;
        }

        const int absX = this->getPosX() + relX;
        const int absY = this->getPosY() + relY;

        //bounds check
        if (absX >= board.getWidth() || absX < 0) {
          return true;
        }
        if (absY >= board.getHeight() || absY < 0) {
          return true;
        }

        //collision check
        if (board.get(absX, absY) != 0) {
          return true;
        }
      }
    }

    return false;
  }

  void place(Board& board) {
    if (this->collidesWithBoard(board)) {
      return;
    }

    const Grid& shape = this->getShapeGridByChar(this->getBagChar(0));
    
    for (int relX = 0; relX < shape.width; relX++) {
      for (int relY = 0; relY < shape.height; relY++) {
        const int currValue = shape.get(relX, relY, rot);
        if (currValue == 0) {
          continue;
        }

        board.set(this->getPosX() + relX, this->getPosY() + relY, currValue);
      }
    }

    this->holdAvailable = true;
    this->next();
  }

  bool move(Board& board, int x, int y) {
    const int oldX = this->posX;
    const int oldY = this->posY;

    this->posX += x;
    this->posY += y;

    if (this->collidesWithBoard(board)) {
      this->posX = oldX;
      this->posY = oldY;
      return false;
    }

    this->performedTSpin = false;
    return true;
  }

  bool rotate(Board& board, int rotationDelta) {
    if (rotationDelta % 4 == 0) {
      return false;
    };
    
    const char currentPiece = this->getBagChar(0);

    if (currentPiece == 'O') {
      return false;
    }

    const int oldRot = this->rot;
    const int newRot = (this->rot + rotationDelta) % 4;

    bool clockwise;
    if (rotationDelta % 2 == 0) {
      clockwise = true; 
    } else {
      clockwise = (rotationDelta == 1);
    }

    const int (*kickTable)[5][2];
    if (currentPiece == 'I') {
      kickTable = srsIKicks;
    } else {
      kickTable = srsKicks;
    }

    const int tableIndex = clockwise ? oldRot : newRot;

    for (int i = 0; i < 5; i++) {
      int dx = kickTable[tableIndex][i][0];
      int dy = -kickTable[tableIndex][i][1];

      bool invert = false;
      
      if (rotationDelta % 2 != 0) {
        if (!clockwise) {
          invert = true;
        }
        if ((oldRot == 1 && newRot == 2) || (oldRot == 3 && newRot == 0)) {
          if ( (clockwise && (oldRot == 1 || oldRot == 3)) || (!clockwise && (oldRot == 0 || oldRot == 2)) ) {
            invert = !invert;
          }
        }
      } 

      if (invert) {
        dx = -dx;
        dy = -dy;
      }

      this->posX += dx;
      this->posY += dy;
      this->rot = newRot; 

      if (!this->collidesWithBoard(board)) {
        this->performedTSpin = this->tSpinEligible(board);
        return true; 
      }

      this->posX -= dx;
      this->posY -= dy;
      this->rot = oldRot;
    }

    this->rot = oldRot;
    return false;
  }

  bool isGrounded(Board& board) {
    const Grid& shape = this->getShapeGridByChar(this->getBagChar(0));
    
    for (int relX = 0; relX < shape.width; relX++) {
      for (int relY = 0; relY < shape.height; relY++) {
        if (shape.get(relX, relY, this->getRot()) == 0) {
          continue;
        }

        const int absX = this->getPosX() + relX;
        const int absY = this->getPosY() + relY;

        //floor check
        if (absY+1 >= board.getHeight()) {
          return true;
        }

        //collision check
        if (board.get(absX, absY+1) != 0) {
          return true;
        }
      }
    }

    return false;
  }

  bool attemptAutoLock(Board& board) {
    const bool newGrounded = this->isGrounded(board);
    if (newGrounded && this->grounded) {
      if (millis() > this->lastGrounded+500) {
        this->place(board);
        this->grounded = false; //will update next frame if grounded on spawn
        return true;
      }
    } else if (newGrounded && !this->grounded) {
      this->lastGrounded = millis();
    }
    this->grounded = newGrounded;

    return false;
  }

  bool attemptFall(Board& board) {
    if (millis() > this->nextFall) {
      this->move(board, 0, 1);
      this->nextFall = millis() + 1000;
      return true;
    }

    return false;
  }

  bool lastMoveWasTSpin() {
    return this->performedTSpin;
  }

  void render(Renderer& renderer) {
    const Grid& shape = this->getShapeGridByChar(this->getBagChar(0));

    for (int relX = 0; relX < shape.width; relX++) {
      for (int relY = 0; relY < shape.height; relY++) {
        const int currValue = shape.get(relX, relY, rot);
        if (currValue == 0) {
          continue;
        }

        renderer.renderPixel(this->getPosX() + relX, this->getPosY() + relY, currValue);
      }
    }
  }
};

class InputHandler {
private:
public:
  class Input {
    private:
      bool ready;
      bool held;
      int repeatTime;
      unsigned long debouce;
      unsigned long lastReady;
    public:
      Input(int repeatTimeMillis) {
        this->ready = false;
        this->held = false;
        this->repeatTime = repeatTimeMillis;
        this->debouce = 0;
        this->lastReady = 0;
      }

      void update(bool pressed) {
        if (!pressed) {
          this->held = false;
          return;
        }
        
        if (!this->held) {
          this->held = true;
          this->debouce = millis() + 100;
          this->lastReady = millis();
          this->ready = true;
          return;
        } else {
          if (this->repeatTime < 0) { //negative repeatTime prevents repeat inputs on hold
            return;
          }

          if (millis() < this->debouce) {
            return;
          }

          if (millis() < this->lastReady + this->repeatTime) {
            return;
          } else {
            this->lastReady = millis();
            this->ready = true;
            return;
          }
        }
      }

      bool pop() {
        const bool temp = this->ready;
        this->ready = false;
        return temp;
      }
  };
  
  Input* left;
  Input* right;
  Input* rotateCW;
  Input* rotateCCW;
  Input* rotate180;
  Input* hardDrop;
  Input* softDrop;
  Input* hold;

  InputHandler(int repeatTime) {
    this->left = new Input(repeatTime);
    this->right = new Input(repeatTime);
    this->rotateCW = new Input(-1);
    this->rotateCCW = new Input(-1);
    this->rotate180 = new Input(-1);
    this->hardDrop = new Input(-1);
    this->softDrop = new Input(repeatTime/2);
    this->hold = new Input(-1);
  }

  ~InputHandler() {
    delete this->left;
    delete this->right;
    delete this->rotateCW;
    delete this->rotateCCW;
    delete this->rotate180;
    delete this->hardDrop;
    delete this->softDrop;
    delete this->hold;
  }
};

InputHandler* inputHandler;
Renderer* renderer;
Piece* piece;
Board* board;
XboxController controller;

void setup() {
  Serial.begin(115200);

  inputHandler = new InputHandler(100);
  renderer = new Renderer();
  piece = new Piece();
  board = new Board();

  delay(5000);
  Serial.println("starting connection period");
  controller.begin();

  delay(5000);
  Serial.println("ending connection period");

  board->render(*renderer);
  piece->render(*renderer);
}

const int FRAMERATE = 30;
unsigned long nextFrame = 0;
void loop() {
  if (!controller.isConnected()) {
    return;
  }

  XboxControlsEvent e;
  controller.read(&e);
  inputHandler->left->update(e.dpadRight);
  inputHandler->right->update(e.dpadLeft);
  inputHandler->rotateCW->update(e.buttonA);
  inputHandler->rotateCCW->update(e.buttonB);
  inputHandler->rotate180->update(e.buttonX);
  inputHandler->hardDrop->update(e.dpadUp);
  inputHandler->softDrop->update(e.dpadDown);
  inputHandler->hold->update(e.buttonY);

  if (micros() > nextFrame) {
    Serial.println("frame stepped");
    nextFrame = micros() + 1000000/FRAMERATE;

    bool updated = false;
    bool tSpin = false;

    if (inputHandler->left->pop()) {
      updated = updated || piece->move(*board, -1, 0);
    }

    if (inputHandler->right->pop()) {
      updated = updated || piece->move(*board, 1, 0);
    }

    if (inputHandler->softDrop->pop()) {
      if (piece->move(*board, 0, 1)) {
        updated = true;
        piece->resetFallTimer(2000);
        board->awardPoints(1);
      }
    }

    if (inputHandler->hardDrop->pop()) {
      updated = true;
      int moved = 0;
      while (piece->move(*board, 0, 1)) {
        moved += 1;
      }
      piece->place(*board);
      board->awardPoints(moved*2);
    }

    if (inputHandler->rotateCW->pop()) {
      updated = updated || piece->rotate(*board, 1);
    }

    if (inputHandler->rotateCCW->pop()) {
      updated = updated || piece->rotate(*board, 3);
    }

    if (inputHandler->rotate180->pop()) {
      updated = updated || piece->rotate(*board, 2);
    }

    if (inputHandler->hold->pop()) {
      updated = updated || piece->swapHold();
      piece->resetFallTimer(2000);
    }

    updated = updated || piece->attemptFall(*board);
    updated = updated || piece->attemptAutoLock(*board);

    if (updated) {
      Serial.println("display updated");
      matrix.clear();
      board->clearFullLines(piece->lastMoveWasTSpin());
      board->render(*renderer);
      piece->render(*renderer);
      matrix.show();
    }
  }
}