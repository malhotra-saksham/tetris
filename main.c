#include <lpc17xx.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <RTL.h>
#include <time.h>
#include "GLCD.h" 

 
OS_SEM newShapeLock;																		//Semaphores for each task (correspond to tasks with the same name)
OS_SEM configLock;
OS_SEM gameStateLock;
OS_MUT joystickLock;
OS_MUT pauseLock;

OS_TID pauseTask;																				//Task ID for pause task

int angle=0;	
typedef struct {																				//Struct containing 4 reference points for a shape
	uint16_t xR[4];
	uint16_t yR[4];
} shape;


void displayBlock(int xR, int yR){											//Display 1 block (4 blocks per shape)
	int xs=xR-8,xe=xR+8;
	int ys=yR-8,ye=yR+8;
	int j=xs, i=ys;
	
	for (i = xs; i < xe; i++){
		for(j = ys; j < ye; j++) {
		GLCD_PutPixel(i,j);
		}
	}
}

void displayShape(shape currShape){											//Display 1 shape
	int i=0;
	for (i = 0; i < 4; i++){
		displayBlock(currShape.xR[i],currShape.yR[i]);
	}
}


shape createShape(int xStart, int yStart, char type){		//Function to create the 4 reference points for a shape (based on type)
	shape currShape;
	int x[4];
	int y[4];
	int i=0,j=0,temp=0;
	x[0] = xStart;
	y[0] = yStart;
	x[1] = xStart;
	y[1] = yStart + 16;
	if (type == 'y') {
		x[2] = xStart + 16;
		y[2] = yStart;
		x[3] = xStart + 16;
		y[3] = yStart + 16;
	}
	else {
		if (type == 'r') {
			x[2] = xStart - 16;
			y[2] = yStart;
			x[3] = xStart - 16;
			y[3] = yStart - 16;
		}
		if (type == 'g') {
			x[2] = xStart + 16;
			y[2] = yStart;
			x[3] = xStart + 16;
			y[3] = yStart - 16;
		}
		if (type == 'b') {
			x[2] = xStart;
			y[2] = yStart - 16;
			x[3] = xStart;
			y[3] = yStart - 32;
		}
		if (type == 'i') {
			x[2] = xStart + 16;
			y[2] = yStart;
			x[3] = xStart + 32;
			y[3] = yStart;
		}
		if (type == 'p') {
			x[2] = xStart - 16;
			y[2] = yStart;
			x[3] = xStart + 16;
			y[3] = yStart;
		}
		if (type == 'o') {
			x[2] = xStart - 16;
			y[2] = yStart;
			x[3] = xStart - 32;
			y[3] = yStart;
		}
		
		for (j=0;j<angle;j++){
			for (i=1;i<4;i++){
				temp = x[i]-x[0];
				x[i] = x[0]-(y[i]-y[0]);
				y[i] = y[0]+temp;
			}
		}
	}
	for (i = 0; i < 4; i++){
		currShape.xR[i] = x[i];
		currShape.yR[i] = y[i];
	}
	return currShape;
}





shape currentShape;																			//Global variables
char currentLetter;
unsigned short currentColour;
uint16_t fullRowMask = 0x7fff;
uint16_t landscape[18] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int xStart=8+(16*7), yStart=8+(16*16), ycount=0, currentCount=0, x, y, score=0, pauseCount=3;
	
bool checkPosition(bool checkOnly,int checknum){				//Checks next potential position of current shape
	int i=0,j=0;
	int landscapeCopy;
	int currentX=0, currentY=0;
	
	for (i = 0; i < 4; i++){
		currentX = (currentShape.xR[i] - 8)/16;
		currentY = (currentShape.yR[i] - 8)/16;
		//printf("landscape for %d: %d, %d\n", currentY,landscape[currentY],(1<<currentX));
		
		if (checkOnly)
			landscapeCopy = landscape[currentY];
		else
			landscapeCopy = landscape[currentY-1];
		
		if ((landscapeCopy & (1<<currentX))||(currentY == checknum)){
			
			if(!checkOnly){
				for (j = 0; j < 4; j++) {
					currentX = (currentShape.xR[j] - 8)/16;
					currentY = (currentShape.yR[j] - 8)/16;
					landscape[currentY] |= (1<<currentX);
					//printf("********\n");
				}
			}
			return false;
		}
	}
	//printf("********\n");
	return true;
	
}

bool checkEdges(){																			//Checks if the CFS is at an edge
	int i=0;
	int currentX=0;
	
	for (i = 0; i < 4; i++){
		currentX = (currentShape.xR[i] - 8)/16;
		
		if (currentX < 0 || currentX > 14)
			return false;
	}
	return true;
}

void displayScore(int num){															//Displays score on LEDs in Binary
		// array to store binary number
    int bNum[10];
		int n = num; 
		int setbit;
	
    // counter for binary array
    int i = 0;
		int j = 0;
    while (n > 0) {
 
        // storing remainder in binary array
        bNum[i] = n % 2;
        n = n / 2;
        i++;
    }
		
		//Set bits
		for (j = 0; j <= 8; j++) {
			if (j < 5){
				setbit = 6-j;
				if (bNum[j] == 1)
					LPC_GPIO2->FIOSET |= (1<<setbit);
				else LPC_GPIO2->FIOCLR |= (1<<setbit);
			}
			else {
				if (j == 5)
					setbit = 31;
				else if (j == 6)
					setbit = 29;
				else if (j == 7)
					setbit = 28;
				
				if (bNum[j] == 1) 
					LPC_GPIO1->FIOSET |= (1<<setbit);
				else LPC_GPIO1->FIOCLR |= (1<<setbit);
			}
		}
}
void shiftLandscape(uint8_t i,int top){
	int j,k=0;
	for (j=i;j<top;j++){
		landscape[j]=landscape[j+1];
		for(k=0;k<15;k++){
			if(landscape[j] & (1<<k))
				GLCD_SetTextColor(White);
			else
				GLCD_SetTextColor(Black);
			displayBlock(8+(k*16),8+(j*16));
		}
	}
}


__task void cfsConfig(void);														//Update Position and Configuration of Current Falling Shape (CFS)
__task void newCFS(void);																//Create a new randomized CFS
__task void gameState(void);														//Updates landscape and score after a CFS reaches the landscape
__task void joystick(void);															//Handles moving the block left, right and down as well as rotation of the block
__task void pause(void);																//Task that handles pausing the game when INT0 button is pushed

__task void start_tasks(void){													//Task/Semaphore initialization
	os_sem_init (&newShapeLock, 1);	
	os_sem_init (&configLock, 0);
	os_sem_init (&gameStateLock, 0);
	os_mut_init (&joystickLock);
	os_mut_init (&pauseLock);
	os_tsk_create(cfsConfig,3);
	os_tsk_create(newCFS,5);
	os_tsk_create(joystick,1);
	os_tsk_create(gameState,4);
	pauseTask = os_tsk_create(pause,20);
	
	for(;;);
}



__task void newCFS(void) {
	while (1) {
		int randomNum = 0;
		char letter[] = "bgropiy";
		os_mut_wait(&pauseLock,0xFFFF);
		os_mut_release(&pauseLock);
		os_sem_wait (&newShapeLock, 0xFFFF);	
	
		randomNum = rand() % 7; 
		//printf("r: %d\n", randomNum);
		
		// use letter[r] as random block selection
		
		currentLetter = letter[randomNum];
	
 		if (currentLetter == 'p')
 			currentColour = Magenta;
 		else if (currentLetter == 'b')
			currentColour = Cyan;
		else if (currentLetter == 'g')
			currentColour = Green;
		else if (currentLetter == 'r')
			currentColour = Red;
		else if (currentLetter == 'y')
			currentColour = Yellow;
		else if (currentLetter == 'i')
			currentColour = Blue;
		else if (currentLetter == 'o')
			currentColour = Orange;
		
		ycount=0;
		x = xStart;
		y = yStart + ycount;
		angle=0;
		GLCD_SetTextColor(currentColour);
		currentShape = createShape(x,y,currentLetter);
		displayShape(currentShape);
		os_sem_send (&configLock);
		currentCount += 2;
	}
}

__task void cfsConfig(void) {
																		// periodically wake up this task every 200 ticks (1 second)
	while(1){
		os_sem_wait (&configLock, 0xFFFF);
		while(checkPosition(false,0)){
			os_mut_wait(&pauseLock,0xFFFF);
			os_mut_release(&pauseLock);
			os_itv_set(200);
			GLCD_SetTextColor(Black);
			currentShape = createShape(x,y,currentLetter);
			displayShape(currentShape);
			y -= 16;
			GLCD_SetTextColor(currentColour);
			currentShape = createShape(x,y,currentLetter);
			displayShape(currentShape);
			os_mut_release(&joystickLock);
			os_itv_wait();																//suspend this task until next wake-up
			os_mut_wait(&joystickLock,0xFFFF);
		}
		GLCD_SetTextColor(White);
		displayShape(currentShape);
		os_sem_send (&gameStateLock);
	}
}
__task void joystick(void) {
	uint32_t bus = LPC_GPIO1->FIOPIN;
	uint32_t dir_masked_bus = bus & 0x7800000;
	shape tempShape;
	int newX=x,newY=y,checknum=0,prevAngle=0;
	bool rotation=false;
	while(1){
		os_mut_wait(&pauseLock,0xFFFF);
		os_mut_release(&pauseLock);
		os_mut_wait (&joystickLock,0xFFFF);
		newX=x;
		newY=y;
		rotation=0;
		prevAngle=angle;
		checknum=0;
		bus = LPC_GPIO1->FIOPIN;											//reads register for input
		dir_masked_bus = bus & 0x7800000;
		
		if (dir_masked_bus == 0x6800000) {										//up
			rotation = 1;																				//cw
		} 
		else if (dir_masked_bus == 0x3800000) {								//down
			newY=y-16;																					//ccw
			checknum=0;
		} 
		else if (dir_masked_bus == 0x7000000) {								//left
			newX=x+16;
			checknum=-1;
		}
		else if (dir_masked_bus == 0x5800000) {								//right
			newX=x-16;   	
			checknum=-1;
		}
		
		if (rotation){
			angle += 1;
		
			angle %= 4;
		}
		
		if (!rotation)
			currentShape = createShape(newX,newY,currentLetter);
		else {
			tempShape = currentShape;
			currentShape = createShape(x,y,currentLetter);
				
			if (checkPosition(true,0) && checkEdges()){
				currentShape = tempShape;
				GLCD_SetTextColor(Black);
				displayShape(currentShape);
				currentShape = createShape(x,y,currentLetter);
				GLCD_SetTextColor(currentColour);
				displayShape(currentShape);
			}
			else {
				currentShape = tempShape;
				angle = prevAngle;
			}
		}
		
		if (checkPosition(true,checknum) && checkEdges() && (newX != x || newY != y) && newY > 0){
				currentShape = createShape(x,y,currentLetter);
				GLCD_SetTextColor(Black);
				displayShape(currentShape);
				x = newX;
				y = newY;
				GLCD_SetTextColor(currentColour);
				currentShape = createShape(x,y,currentLetter);
				displayShape(currentShape);
		}
		else {
			currentShape = createShape(x,y,currentLetter);
		}
		os_mut_release(&joystickLock);
	}
}
__task void gameState(void) {
	uint8_t i=0,top=17,j=0;
	int row = 88,col = 98;
	short fi = 1;
	while(1)	{
		top=0;
		j=0;
		os_mut_wait(&pauseLock,0xFFFF);
		os_mut_release(&pauseLock);
		os_sem_wait (&gameStateLock, 0xFFFF);
		while ((landscape[j] & fullRowMask)>0){
			top++;
			j++;
	  }
		for(i=0;i<top;i++){
		 if ((landscape[i] & fullRowMask) == fullRowMask){
				score++;
				displayScore(score);
				shiftLandscape(i,top);
				i--;
				top--;
				printf("Score: %d\n", score);
		 }
		}
		
		if(top > 17){
				GLCD_Init1();
				GLCD_Clear(Black);
				GLCD_SetBackColor(Black);
				GLCD_SetTextColor(White);
				GLCD_DisplayString(row, col+3.5, fi, "GAME");
				GLCD_DisplayString(row+1, col+3.5, fi, "OVER");
				GLCD_DisplayString(row-3, col-4, fi-1, "Press Reset to play again");
			while(1);
		}
		if(score >= 250){
				GLCD_Init1();
				GLCD_Clear(Black);
				GLCD_SetBackColor(Black);
				GLCD_SetTextColor(White);
				GLCD_DisplayString(row, col+4, fi, "YOU");
				GLCD_DisplayString(row+1, col+4, fi, "WIN");
				GLCD_DisplayString(row-3, col-4, fi-1, "Press Reset to play again");
			while(1);
		}
		os_sem_send (&newShapeLock);
	}		
}
__task void pause(void) {
	int i=0;
	os_itv_set(20);
	while(1) {
		os_mut_wait(&pauseLock,0xFFFF);
		if(pauseCount<=0){
			printf("No more pauses remaining\n");
			os_mut_release(&pauseLock);
			os_tsk_delete(pauseTask);
		}
		if (!(LPC_GPIO2->FIOPIN & 0x400)){
			pauseCount--;
			printf("Pauses Remaining: %d\n", pauseCount);
			for(i=0; i<50;i++){
				os_itv_wait();
			}
		}
		os_itv_wait();
		os_mut_release(&pauseLock);
	}
}



int main(void){
	
	unsigned long randomNumber = 20000000;
	int row = 88;
	int col = 98;
	short fi = 1;
	
	printf("Starting\n");
	
	LPC_GPIO2->FIODIR |= 0x0000007C;
	LPC_GPIO1->FIODIR |= 0xB0000000;

	// Welcome Screen Code
	GLCD_Init1();
	GLCD_Clear(Black);
	GLCD_SetBackColor(Black);
	GLCD_SetTextColor(White);
	GLCD_DisplayString(row, col, fi, "T E T R I S");
	GLCD_DisplayString(row-3, col-4, fi-1, "Press INT0 Button to Play");
	GLCD_DisplayString(row+12, col+9, fi-1, "Programmed by:");
	GLCD_DisplayString(row+13, col+9, fi-1, "Saksham Malhotra");
	
	//Randomization of seed for rand() function
	while((LPC_GPIO2->FIOPIN & 0x400)){
		randomNumber++;
	}
	srand(randomNumber);
	
	//Reinitialization of GLCD for game orientation
	GLCD_Init2();
	GLCD_Clear(Black);

	os_sys_init(start_tasks);				//Initialize Tasks
}

