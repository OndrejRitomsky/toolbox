/*
@author Ondrej Ritomsky
@version 0.13, 09.01.2019

No warranty implied
*/

#pragma once


struct OmGuiContext;

typedef void*(*GuiAllocateFn)(void* allocatorContext, unsigned int size);
typedef void(*GuiDeallocateFn)(void* allocatorContext, void* mem);

struct OmGuiIAllocator {
	GuiAllocateFn Allocate;
	GuiDeallocateFn Deallocate;
};


enum OmGuiKeyType : unsigned char { // Just IsDown!
	// 0 - 128 is currently done by WM_CHAR and maps 1 to 1
	OMGUI_KEY_INVALID = 128,
	OMGUI_KEY_DELETE,
};


enum OmGuiCursorType : unsigned char {
	OMGUI_CURSOR_INVALID = 0,
	OMGUI_CURSOR_NORMAL, // default
	OMGUI_CURSOR_IBEAM,
	OMGUI_CURSOR_POINTER,
	OMGUI_CURSOR_MOVE_HORIZONTAL,
	OMGUI_CURSOR_MOVE_VERTICAL,
	OMGUI_CURSOR_MOVE_DIAGONAL,
	OMGUI_CURSOR_MOVE_DIAGONAL_ALT
};


struct OmGuiStyle {
	unsigned int tabBackgroundColor;

	unsigned int tabHeaderColor;
	unsigned int tabHeaderHotColor;

	unsigned int borderColor;
	unsigned int borderResizingColor;

	unsigned int textFieldHighlightColor;

	unsigned int buttonColor;
	unsigned int buttonHotColor;
	unsigned int textColor;

	int tabElementsIndent;
	int tabElementsSpacing;

	int tabBorderWidth;
	int borderCheckSize;

	int scrollbarWidth;
	int elementBorderWidth; 

	int labelIndent;
	int fontSize;
};

// Fill every frame for input
struct OmGuiInput {
	bool clickDown;
	bool clickUp;
	int mouseX;
	int mouseY;
	int windowWidth;
	int windowHeight;
	unsigned int inputCount;
	const OmGuiKeyType* inputData;
};




void OmGuiContextMemoryInfo(unsigned int* outMemSize, unsigned int* outAllignmentSize);

OmGuiStyle OmGuiDefaultStyle();

// buffer must match OmGuiContextMemoryInfo function
OmGuiContext* OmGuiPlacementInit(void* buffer, OmGuiStyle* style, OmGuiIAllocator* allocator);

void OmGuiDeinit(OmGuiContext* context);

void OmGuiUpdateInput(OmGuiContext* context, const OmGuiInput* input);

// returns permanent ID
unsigned int OmGuiAddTab(OmGuiContext* context, const char* name);


void OmGuiSetCurrentTab(OmGuiContext* context, unsigned int tabId);

bool OmGuiButton(OmGuiContext* context, const char* text);

void OmGuiTextField(OmGuiContext* context, char* text, unsigned char maxSize, unsigned char viewSize);

int OmGuiIntField(OmGuiContext* context, int value, int minValue, int maxValue, unsigned char maxSize, unsigned char viewSize);

float OmGuiFloatField(OmGuiContext* context, float value, float minValue, float maxValue, unsigned char maxSize, unsigned char viewSize);

void OmGuiLabel(OmGuiContext* context, const char* text);

bool OmGuiSubTab(OmGuiContext* context, const char* text);

int OmGuiSlider(OmGuiContext* context, int minValue, int maxValue, int value);

bool OmGuiCheckbox(OmGuiContext* context, bool enabled);

// Subrow isnt supported
void OmGuiRow(OmGuiContext* context);

// 0 -> max
void OmGuiCanvas(OmGuiContext* context, int width, int height, unsigned char id);

// Subtable isnt supported, columnsCount must be > 0
// to skip rest of row call OmGuiRow
void OmGuiTable(OmGuiContext* context, unsigned int columnsCount);

// Will automaticaly do new row
void OmGuiTableEnd(OmGuiContext* context);

// the buffer will be invalidate with any other omgui call
char* OmGuiUpdate(OmGuiContext* context, unsigned int* outBufferSize, OmGuiCursorType* optOutCursor);

unsigned int OmGuiColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

void OmGuiColorToFloats(int color, float outRgb[3]);






// -------------------------- RENDER COMMANDS -----------------------
enum OmGuiCommandType : unsigned char {
	OMGUI_COMMAND_NONE = 0, // NEVER SENT
	OMGUI_COMMAND_RECT,
	OMGUI_COMMAND_TRIANGLE,
	OMGUI_COMMAND_CSTRING,
	OMGUI_COMMAND_SCISSOR_ON,
	OMGUI_COMMAND_SCISSOR_OFF,
	OMGUI_COMMAND_USER_CANVAS,
};

// There is check in cpp if the alignment isnt higher
// This is for the buffer with commands has properly aligned data and doesnt need another list of pointers to the buffer 
// just buffer += sizeof(....ComandData) and alignment should be proper

// The way of render commands might be changed in the future

__declspec(align(8))
struct OmGuiRectCommandData {
	OmGuiCommandType type;
	int x, y, w, h;
	int color;
};

__declspec(align(8))
struct OmGuiTriangleCommandData {
	OmGuiCommandType type;
	int x, y;
	int rotDeg;
	int size;
	int color;
};

__declspec(align(8))
struct OmGuiCStringCommandData {
	OmGuiCommandType type;
	int x, y;
	int color;
	const char* text;
};

__declspec(align(8))
struct OmGuiScissorOnCommandData {
	OmGuiCommandType type;
	int x, y, w, h;
};

__declspec(align(8))
struct OmGuiScissorOffCommandData {
	OmGuiCommandType type;
};


__declspec(align(8))
struct OmGuiUserCanvasCommandData {
	OmGuiCommandType type;
	unsigned char id;
	int x, y, w, h;
};
