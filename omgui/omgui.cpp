#include "omgui.h"

#include <string.h> // strlen ..
#include <math.h>   // roundf
#include <stdio.h>
#include <stdlib.h>

// @TODO not refreshing tab should be marked!
// @TODO not updating tab next round should be resolved.. dont render the if its shown? render it if its hidden ... then if shown dont? what about active or only one last tab

// @TODO Table resizing on tab changing (reason: Columns width set to 0 -> first recalculate has bad value)

// @TODO tree 

// @TODO strlen shouldnt be called all the time .. only in initial creation then stored (not realy needed for now)

// @TODO is elementState needed?, what states transitions needed to interact with input


#define PARANOIA_LEVEL 0


#if PARANOIA_LEVEL >= 1
#include <assert.h>
#define Assert(val) assert(val);
#else
#define Assert(val) ((void*)0);
#endif


typedef unsigned char u8;
typedef unsigned int u32;
typedef int i32;
typedef float f32;

template<typename Type>
struct OmGui_Array {
	u32 count_;
	u32 _capacity;
	Type* _data;

	Type& operator[](u32 index);
	const Type& operator[](u32 index) const;

	Type* begin();
	Type* end();

	const Type* begin() const;
	const Type* end() const;
};

struct OmGui_Style : OmGuiStyle {
	i32 _signPrimarySize;
	i32 _signSecondarySize;
	i32 _fontSizeW;
};

enum OmGui_BorderFlags : u8 {
	OMGUI_NONE_FLAG = 0,
	OMGUI_UP_FLAG = 1,
	OMGUI_RIGHT_FLAG = 2,
	OMGUI_DOWN_FLAG = 4,
	OMGUI_LEFT_FLAG = 8
};


enum OmGui_TabState : u8 {
	OMGUI_TAB_STATE_INVALID = 0,
	OMGUI_TAB_STATE_NORMAL,
	OMGUI_TAB_STATE_MOVING,
	OMGUI_TAB_STATE_RESIZING,
	OMGUI_TAB_STATE_SCROLLING,
	OMGUI_TAB_STATE_UNDOCKING
};

enum OmGui_ElementState : u8 {
	OMGUI_ELEMENT_STATE_INVALID = 0,
	OMGUI_ELEMENT_STATE_NORMAL,
	OMGUI_ELEMENT_STATE_EDITING,
	OMGUI_ELEMENT_STATE_SLIDING,
};

struct OmGui_Input : OmGuiInput {
	i32 oldMouseX;
	i32 oldMouseY;

	u32 bufferInput; // offset
};

struct OmGui_Rect {
	i32 x;
	i32 y;
	i32 w;
	i32 h;
};

struct OmGui_Tab;

struct OmGuiContext {
	OmGuiCursorType cursor;
	OmGui_ElementState elementState;
	OmGui_TabState tabState;
	OmGui_TabState nextTabState;

	bool activeTabHasCursor;
	bool hasOverlay; // dropdown ...
	bool rotateActive; // only textfield now

	u32 currentElement;  // the one user is currently pushing, not used in update/render function

	u32 hotElementIndex;
	u32 activeElementIndex;
	u32 hotTabIndex;
	u32 nextActiveTabIndex;

	u32 bufferDataSize;
	u32 bufferDataCapacity;
	char* bufferData; // this cant be used (cant do reallocation) so render data can point here, so the render string command is always valid pointer

	u32 renderDataSize;
	u32 renderDataCapacity;
	char* renderData;


	OmGuiIAllocator* allocator;

	OmGui_Style style; // wrap to public types
	OmGui_Input input; // wrap to public types

	OmGui_Tab* currentTab;  // the one user is currently pushing elements into, not used in update/render function

	OmGui_Array<u32> tabIDToTabHandle; // permanent currently, cant be moved (@TODO lookuparray)
	OmGui_Array<OmGui_Tab> tabs;

	char editBuffer[64];
};



enum OmGui_ElementType : u8 {
	OMGUI_INVALID,
	OMGUI_LABEL,
	OMGUI_BUTTON,
	OMGUI_TEXT_FIELD,
	OMGUI_U32_FIELD,
	OMGUI_I32_FIELD,
	OMGUI_F32_FIELD,
	OMGUI_NEW_ROW,
	OMGUI_SUBTAB,
	OMGUI_DROPDOWN, // only for tab now
	OMGUI_SLIDER,
	OMGUI_CANVAS,
	OMGUI_CHECKBOX,
	OMGUI_TABLE,
	OMGUI_TABLE_END
};

struct OmGui_TableColumn {
	i32 width;
};

struct OmGui_Element {
	OmGui_ElementType type;
	union {
		struct { 
			bool stretchW;
			bool stretchH;
			u8   id; // can be u16
		} canvas;

		struct {
			u8 viewSize;
			u8 viewOffset;
		} field;

		bool check_enabled;  // checkbox, subtab
		i32 dropdown_maxWidth;
		i32 slider_value;
		u32 tableColumnsCount;
	};

	u32 index;
	u32 hash;
	OmGui_Rect rect;

	union {
		u32 bufferDataOffset; // used by fields where buffer has to be allocated, since reallocation, it has to be offset
		const char* text; // used only by buffers from user
	};
};

struct OmGui_TabButton {
	i32 offsetX;
	u32 textSize;
	const char* text;
};

struct OmGui_Tab {
	bool isHidden;
	OmGui_BorderFlags side;
	u32 id;
	u32 nextIndex;
	u32 index;
	u32 chainIndex;
	i32 scrollbarOffset;
	i32 dropDownMaxWidth;

	OmGui_Rect rect;
	OmGui_TabButton button;
	OmGui_Array<OmGui_Element> elements;
};


enum OmGui_FieldEditState {
	OMGUI_FIELD_INVALID = 0,
	OMGUI_FIELD_NO_EDIT,
	OMGUI_FIELD_EDITING,
	OMGUI_FIELD_EDIT_START,
	OMGUI_FIELD_EDIT_CANCELED,
	OMGUI_FIELD_EDIT_FINISHED,
};


static u32 DROPDOWN_BUTTON_INDEX = 0x7FffFFfd; // not in array, maybe move to array and set special TYPE ? (it will be tab header anyway)
static u32 TAB_BUTTON_INDEX = 0x7FffFFfe; // not in array, maybe move to array and set special TYPE ? (it will be tab header anyway)
static u32 INVALID_ARRAY_INDEX = 0x7FffFFff;

const static OmGuiKeyType KEY_TYPE_TAB = (OmGuiKeyType) 0x9;
const static OmGuiKeyType KEY_TYPE_BACKSPACE = (OmGuiKeyType) 0x8;
const static OmGuiKeyType KEY_TYPE_ENTER = (OmGuiKeyType) 0xD;

// ------------------------- ARRAY --------------------------------------------

template<typename Type>
inline void OmGui_ArrayInit(OmGui_Array<Type>& array) {
	array._capacity = 0;
	array._data = nullptr;
	array.count_ = 0;
}

template<typename Type>
inline void OmGui_ArrayDeinit(OmGui_Array<Type>& array, OmGuiIAllocator* allocator) {
	if (array._data) {
		allocator->Deallocate(allocator, array._data);
		array._data = nullptr;
	}
}

template<typename Type>
inline bool OmGui_ArrayAdd(OmGui_Array<Type>& array, const Type& element, OmGuiIAllocator* allocator) {
	u32 reqCapacity = array.count_ + 1;
	if (reqCapacity > array._capacity) { // Reserve
		u32 capacity = array._capacity * 2;
		if (capacity < reqCapacity)
			capacity = reqCapacity;

		Type* data = (Type*) allocator->Allocate(allocator, capacity * sizeof(Type));
		if (!data)
			return false;

		if (array._data) {
			memcpy(data, array._data, array.count_ * sizeof(Type));
			allocator->Deallocate(allocator, array._data);
		}

		array._data = data;
		array._capacity = capacity;
	}

	memcpy(array._data + array.count_, &element, sizeof(Type));
	++array.count_;

	return true;
}


template<typename Type>
inline void OmGui_ArrayRemoveSwap(OmGui_Array<Type>& array, u32 index) {
	Assert(0 < array.count_);
	Assert(index < array.count_);
	array._data[index] = array._data[array.count_ - 1];
	--array.count_;
}

template<typename Type>
inline void OmGui_ArrayRemoveOrdered(OmGui_Array<Type>& array, u32 index) {
	Assert(0 < array.count_);
	Assert(index < array.count_);
	memmove(&array._data[index], &array._data[index + 1], (array.count_ - 1 - index) * sizeof(Type));
	--array.count_;
}

template<typename Type>
inline Type& OmGui_Array<Type>::operator[](u32 index) {
	Assert(index < count_);
	return _data[index];
}


template<typename Type>
inline const Type& OmGui_Array<Type>::operator[](u32 index) const {
	Assert(index < count_);
	return _data[index];
}

template<typename Type>
inline Type* OmGui_Array<Type>::begin() {
	return _data;
}

template<typename Type>
inline const Type* OmGui_Array<Type>::begin() const {
	return _data;
}

template<typename Type>
inline Type* OmGui_Array<Type>::end() {
	return _data + count_;
}

template<typename Type>
inline const Type* OmGui_Array<Type>::end() const {
	return _data + count_;
}

// ------------------------- UTIL ---------------------------------------------

u32 OmGui_Hash(const char* text);

i32 OmGui_Max(i32 a, i32 b);

bool OmGui_PointInRect(i32 px, i32 py, i32 rx, i32 ry, i32 w, i32 h);

bool OmGui_PointInGuiRect(i32 px, i32 py, const OmGui_Rect* rect);

i32 OmGui_Clamp(i32 value, i32 min, i32 max);

void OmGui_Swap(u32* a, u32* b);

OmGui_Rect OmGui_RectCreate(i32 x, i32 y, i32 w, i32 h);

// ------------------------- TAB ----------------------------------------------
//@TODO clean 

bool OmGui_TabIsActive(const OmGuiContext* context, const OmGui_Tab* tab) {
	return context->tabs[context->tabs.count_ - 1].index == tab->index;
}

void OmGui_TabElementSetHot(OmGuiContext* context, i32 tabIndex, i32 elementIndex) {
	context->hotTabIndex = tabIndex;
	context->hotElementIndex = elementIndex;
}

bool OmGui_TabElementIsHot(const OmGuiContext* context, i32 tabIndex, i32 elementIndex) {
	return context->hotTabIndex == tabIndex && context->hotElementIndex == elementIndex;
}

bool OmGui_TabElementIsActive(const OmGuiContext* context, i32 tabIndex, i32 elementIndex) {
	return context->tabs[context->tabs.count_ - 1].index == tabIndex && context->activeElementIndex == elementIndex;
}

bool OmGui_ElementAllowInput(const OmGuiContext* context, const OmGui_Tab* tab) {
	return context->hotTabIndex == tab->index && !context->hasOverlay && context->tabState == OMGUI_TAB_STATE_NORMAL && 
		(context->elementState == OMGUI_ELEMENT_STATE_NORMAL || context->elementState == OMGUI_ELEMENT_STATE_EDITING);
}

bool OmGui_ElementRectHot(OmGuiContext* context, const OmGui_Tab* tab, i32 elementIndex, bool setActive, const OmGui_Rect* rect);


// sets giuelement rect and index
OmGui_Element* OmGui_AddElement(OmGuiContext* context, OmGui_ElementType type, const char* text, bool* optOutNew);

void OmGui_TabRenderHeader(OmGuiContext* context, OmGui_Tab* tab, bool highlightHeader);

void OmGui_TabDock(OmGuiContext* context, OmGui_Tab* dockTab, u32 tabIndex, u32 chainIndex);

void OmGui_HiddenTabUndock(OmGuiContext* context, OmGui_Rect rect, u32 hiddenIndex);

void OmGui_HiddenTabSetActive(OmGuiContext* context, u32 tabIndex, u32 hiddenIndex, u32 chainIndex);

void OmGui_TabSetActive(OmGuiContext* context, u32 tabIndex);


i32 OmGui_TabReportRectW(OmGuiContext* context, u32 charCount);

i32 OmGui_TabReportCenteredRectW(OmGuiContext* context, u32 charCount);

i32 OmGui_TabReportRectH(OmGuiContext* context);

i32 OmGui_SubTabReportH(OmGuiContext* context);

i32 OmGui_TableReport(OmGuiContext* context, const OmGui_Tab* tab, const OmGui_Element* table, const OmGui_Element** inOutIterator);

i32 OmGui_TabContentReportHeight(OmGuiContext* context, OmGui_Tab* tab);

OmGui_BorderFlags OmGui_RectBorderCheck(OmGuiContext* context, OmGui_Rect* rect, i32 borderLimit);

void OmGui_RectResize(OmGuiContext* context, OmGui_Rect* rect, OmGui_BorderFlags side, i32 minW, i32 minH, i32 maxW, i32 maxH);

// ------------------------- UPDATE / RENDER ELEMENTS ------------------------- 

OmGui_Rect OmGui_TabDropDownRect(const OmGuiContext* context, const OmGui_Tab* tab);

bool OmGui_TabScrollbar(OmGuiContext* context, OmGui_Tab* tab, i32* outContentMinY);

void OmGui_TabDropDownUpdate(OmGuiContext* context, OmGui_Tab* tab, /* OmGui_Element* element,*/ u32 tabsCount);

void OmGui_TabButtonUpdate(OmGuiContext* context, OmGui_Tab* baseTab, OmGui_Tab* buttonTab);


OmGui_FieldEditState OmGui_FieldUpdate(OmGuiContext* context, OmGui_Element* element, char* buffer, u8 size, u8 maxSize);


void OmGui_TabDropDownRender(OmGuiContext* context, const OmGui_Tab* baseTab, /*const OmGui_Element* element*/ u32 tabsCount);

i32 OmGui_TabButtonWidth(OmGuiContext* context, OmGui_Tab* buttonTab);

void OmGui_TabButtonRender(OmGuiContext* context, OmGui_Tab* baseTab, OmGui_Tab* buttonTab); // @TODO refactor and const?



void OmGui_LabelRender(OmGuiContext* context, const OmGui_Rect* rect, const char* text);

void OmGui_ButtonRender(OmGuiContext* context, const OmGui_Tab* tab, const OmGui_Element* element);

void OmGui_ToggleButtonRender(OmGuiContext* context, const OmGui_Tab* tab, const OmGui_Element* element);

void OmGui_TextFieldRender(OmGuiContext* context, const OmGui_Tab* tab, const OmGui_Element* element);

void OmGui_CheckBoxRender(OmGuiContext* context, const OmGui_Tab* tab, const OmGui_Element* element);

void OmGui_SliderRender(OmGuiContext* context, const  OmGui_Tab* tab, const OmGui_Element* element);


//  futureTabContentWidth canvas needs to know future size, so it doesnt lag behing when reporting to user... @TODO cleanup (maybe correction previous current dif etc, or dont render if resizing)
void OmGui_TabContentRender(OmGuiContext* context, OmGui_Tab* tab, i32 tabContentWidth, i32 futureTabContentWidth, i32 offsetY);

void OmGui_TabRender(OmGuiContext* context, OmGui_Tab* tab);

// setActive == true -> invalidates nextActiveTabIndex
void OmGui_SetActive(OmGuiContext* context, u32 tabIndex, bool setActive);

OmGuiCursorType OmGui_CursorFromBorder(OmGui_BorderFlags border);



// ------------------------- RENDER -----------------------------------------

char* OmGui_ElementBufferData(OmGuiContext* context, const OmGui_Element* element) {
	Assert(element->bufferDataOffset < context->bufferDataSize);
	return context->bufferData + element->bufferDataOffset;
}

// non render data! must be called before any render data has been added
u32 OmGui_PushBufferData(OmGuiContext* context, const char* data, u32 size);

char* OmGui_ContextPushRenderData(OmGuiContext* context, const char* data, u32 size);

// @TODO proper names
void OmGui_RectCommand(OmGuiContext* context, i32 x, i32 y, i32 w, i32 h, i32 color);

void OmGui_TriangleCommand(OmGuiContext* context, i32 x, i32 y, i32 size, i32 rotDeg, i32 color);

void OmGui_CStringCommand(OmGuiContext* context, const char* text, i32 x, i32 y, i32 color);

void OmGui_ScissorOnCommand(OmGuiContext* context, i32 x, i32 y, i32 w, i32 h);

void OmGui_ScissorOffCommand(OmGuiContext* context);

void OmGui_UserCanvasCommand(OmGuiContext* context, i32 x, i32 y, i32 w, i32 h, u8 id);


#if PARANOIA_LEVEL > 0
static void OmGui_DebugVerifyChain(OmGuiContext* context, OmGui_Tab* tab) {
	u32 index = tab->index;
	u32 nextIndex = tab->nextIndex;

	Assert(context->tabIDToTabHandle[tab->id] == tab->index);

	while (nextIndex != INVALID_ARRAY_INDEX) {
		OmGui_Tab* hidden = &context->tabs[nextIndex];
		Assert(nextIndex != index);
		nextIndex = hidden->nextIndex;
		index = hidden->index;
	}
}

#define VerifyChains(context) for (u32 i = 0; i < context->tabs.count_; ++i) OmGui_DebugVerifyChain(context, &context->tabs[i]);
#else 
#define VerifyChains(context) ((void*)0);
#endif


OmGuiStyle OmGuiDefaultStyle() {
	OmGuiStyle style;
	style.fontSize = 16;

	style.tabElementsIndent = 6;
	style.tabElementsSpacing = 8;
	style.labelIndent = 4;
	style.tabBorderWidth = 2;
	style.elementBorderWidth = 2;
	style.borderCheckSize = 8;

	style.tabBackgroundColor = OmGuiColor(27, 34, 54, 255);

	style.tabHeaderColor = OmGuiColor(47, 54, 64, 255);
	style.tabHeaderHotColor = OmGuiColor(63, 69, 82, 255);

	style.borderColor = OmGuiColor(54, 105, 148, 255);
	style.borderResizingColor = OmGuiColor(90, 146, 196, 255);

	style.buttonColor = OmGuiColor(54, 105, 148, 255);
	style.buttonHotColor = OmGuiColor(72, 126, 176, 255);

	style.textColor = OmGuiColor(204, 204, 204, 255);

	style.textFieldHighlightColor = OmGuiColor(47, 54, 64, 255);

	style.scrollbarWidth = 20;

	return style;
}

void OmGuiContextMemoryInfo(unsigned int* outMemSize, unsigned int* outAllignmentSize) {
	*outMemSize = sizeof(OmGuiContext);
	*outAllignmentSize = alignof(OmGuiContext);
}

OmGuiContext* OmGuiPlacementInit(void* buffer, OmGuiStyle* style, OmGuiIAllocator* allocator) {
	OmGuiContext* context = (OmGuiContext*) buffer;
	*context = {};

	context->rotateActive = false;
	context->currentTab = nullptr;
	context->cursor = OMGUI_CURSOR_NORMAL;
	context->input.windowWidth = 1024;
	context->input.windowHeight = 768;
	context->input.clickDown = false;
	context->input.clickUp = false;
	memcpy(&context->style, style, sizeof(OmGuiStyle));
	context->allocator = allocator;
	context->hotElementIndex = INVALID_ARRAY_INDEX;
	context->activeElementIndex = INVALID_ARRAY_INDEX;
	context->hotTabIndex = INVALID_ARRAY_INDEX;
	context->nextActiveTabIndex = INVALID_ARRAY_INDEX;
	context->tabState = OMGUI_TAB_STATE_NORMAL;
	context->nextTabState = OMGUI_TAB_STATE_NORMAL;
	context->elementState = OMGUI_ELEMENT_STATE_NORMAL;
	context->activeTabHasCursor = false;
	context->bufferDataCapacity = 0;
	context->bufferDataSize = 0;
	context->bufferData = nullptr;
	context->renderDataCapacity = 0;
	context->renderDataSize = 0;
	context->renderData = nullptr;
	context->hasOverlay = false;
	context->editBuffer[0] = '\0';

	context->style._signPrimarySize = context->style.fontSize;
	context->style._fontSizeW = context->style.fontSize * 9 / 16;
	context->style._signSecondarySize = context->style._signPrimarySize * 9 / 16;

	OmGui_ArrayInit(context->tabs);
	OmGui_ArrayInit(context->tabIDToTabHandle);

	return context;
}

void OmGuiDeinit(OmGuiContext* context) {
	for (auto& tab : context->tabs) {
		OmGui_ArrayDeinit(tab.elements, context->allocator);
	}

	OmGui_ArrayDeinit(context->tabs, context->allocator);
	OmGui_ArrayDeinit(context->tabIDToTabHandle, context->allocator);

	if (context->bufferData)
		context->allocator->Deallocate(context->allocator, context->bufferData);

	if (context->renderData)
		context->allocator->Deallocate(context->allocator, context->renderData);
}

u32 OmGuiAddTab(OmGuiContext* context, const char* name) {
	OmGui_Tab tab;

	OmGui_ArrayInit(tab.elements);

	tab.id = context->tabIDToTabHandle.count_;
	u32 id = tab.id;

	tab.isHidden = false;
	tab.index = context->tabs.count_;
	tab.nextIndex = INVALID_ARRAY_INDEX;
	tab.rect = OmGui_RectCreate(100, 100, 640, 480);
	tab.button.offsetX = 0;
	tab.button.text = name;
	tab.button.textSize = (u32) strlen(name);
	tab.side = OMGUI_NONE_FLAG;
	tab.scrollbarOffset = 0;
	tab.chainIndex = 0;

	tab.dropDownMaxWidth = 0;

	context->nextActiveTabIndex = tab.index;

	OmGui_ArrayAdd(context->tabs, tab, context->allocator);
	OmGui_ArrayAdd(context->tabIDToTabHandle, tab.index, context->allocator);

	if (tab.index > 0) {
		tab.isHidden = true;
		OmGui_TabDock(context, &context->tabs[0], tab.index, context->tabs.count_ - 1);
		OmGui_TabSetActive(context, 0);
		context->nextActiveTabIndex = context->tabs.count_ - 1;
	}

	return id;
}

void OmGuiUpdateInput(OmGuiContext* context, const OmGuiInput* input) {
	memcpy(&context->input, input, sizeof(OmGuiInput));

	context->input.bufferInput = OmGui_PushBufferData(context, (const char*) context->input.inputData, context->input.inputCount * sizeof(OmGuiKeyType));
	context->input.inputData = nullptr;	

	context->activeTabHasCursor = false;
	u32 count = context->tabs.count_;
	
	if (count > 0) {
		OmGui_Tab* activeTab = &context->tabs[count - 1];
		i32 relativeX = context->input.mouseX - activeTab->rect.x;
		i32 relativeY = context->input.mouseY - activeTab->rect.y;

		i32 size = context->style.borderCheckSize;
		bool activeHasInput = OmGui_PointInRect(relativeX, relativeY, -size, -size, activeTab->rect.w + 2 * size, activeTab->rect.h + 2 * size);

		if (activeHasInput && context->tabState != OMGUI_TAB_STATE_MOVING)
			context->activeTabHasCursor = true;
	}
}

void OmGuiSetCurrentTab(OmGuiContext* context, unsigned int tabId) {
	if (context->currentTab && context->currentElement < context->currentTab->elements.count_)
		context->currentTab->elements.count_ = context->currentElement;

	Assert(tabId < context->tabIDToTabHandle.count_);
	context->currentElement = 0;

	u32 index = context->tabIDToTabHandle[tabId];
	context->currentTab = &context->tabs[index];
}

OmGui_Rect OmGui_RectCreate(i32 x, i32 y, i32 w, i32 h) {
	return { x, y, w, h };
}

unsigned int OmGuiColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
	return (((u32) r & 0xFF) << 24) |
	       (((u32) g & 0xFF) << 16) |
	       (((u32) b & 0xFF) << 8) |
	       (((u32) a & 0xFF));
}

void OmGuiColorToFloats(int color, float outRgb[3]) {
	const f32 v = 1.0f / 256.0f;
	outRgb[0] = v * (f32)((color >> 24) & 0xFF);
	outRgb[1] = v * (f32)((color >> 16) & 0xFF);
	outRgb[2] = v * (f32)((color >> 8) & 0xFF);
}

bool OmGuiButton(OmGuiContext* context, const char* text) {
	OmGui_Element* element = OmGui_AddElement(context, OMGUI_BUTTON, text, nullptr);
	if (OmGui_ElementRectHot(context, context->currentTab, element->index, context->input.clickUp, &element->rect))
		return context->input.clickUp;

	return false;
}

void OmGuiTextField(OmGuiContext* context, char* text, unsigned char maxSize, unsigned char viewSize) {
	OmGui_Element* element = OmGui_AddElement(context, OMGUI_TEXT_FIELD, text, nullptr);
	element->field.viewSize = viewSize;
	element->bufferDataOffset = 0;

	const u8 BUFFER_SIZE = sizeof(context->editBuffer);
	char* editBuffer;
	u8 editBufferSize = maxSize < BUFFER_SIZE ? maxSize : BUFFER_SIZE;
	u8 size;

	if (!OmGui_TabElementIsActive(context, context->currentTab->index, element->index) || context->elementState != OMGUI_ELEMENT_STATE_EDITING) {
		size = (u8) strlen(text);
		editBuffer = text;
	}
	else {
		editBuffer = context->editBuffer;
		size = (u8) strlen(context->editBuffer);
	}

	OmGui_FieldEditState state = OmGui_FieldUpdate(context, element, editBuffer, size, editBufferSize);
	if (state == OMGUI_FIELD_EDIT_FINISHED) {
		memcpy(text, context->editBuffer, editBufferSize);
		context->editBuffer[0] = '\0';
	}
	else if (state == OMGUI_FIELD_EDIT_START) {
		memcpy(context->editBuffer, text, editBufferSize);
	}
	else if (state == OMGUI_FIELD_EDIT_CANCELED) {
		context->editBuffer[0] = '\0';
	}
}


float OmGuiFloatField(OmGuiContext* context, float value, float minValue, float maxValue, unsigned char maxSize, unsigned char viewSize) {
	OmGui_Element* element = OmGui_AddElement(context, OMGUI_F32_FIELD, nullptr, nullptr);
	element->field.viewSize = viewSize;
	element->bufferDataOffset = 0;

	const u8 BUFFER_SIZE = sizeof(context->editBuffer);
	char buffer[sizeof(context->editBuffer)];

	char* editBuffer;
	u8 editBufferSize = maxSize + 1 < BUFFER_SIZE ? maxSize + 1 : BUFFER_SIZE;

	u8 size;

	if (!OmGui_TabElementIsActive(context, context->currentTab->index, element->index) || context->elementState != OMGUI_ELEMENT_STATE_EDITING) {
		editBuffer = buffer;
		size = snprintf(editBuffer, editBufferSize, "%g", value);
		if (size - 1 > editBufferSize)
			size = editBufferSize - 1;
	}
	else {
		editBuffer = context->editBuffer;
		size = (u8) strlen(context->editBuffer);
	}

	OmGui_FieldEditState state = OmGui_FieldUpdate(context, element, editBuffer, size, editBufferSize);

	f32 newValue = value;
	if (state == OMGUI_FIELD_EDIT_FINISHED) {
		newValue = (f32) strtof(context->editBuffer, nullptr);
		context->editBuffer[0] = '\0';
	}
	else if (state == OMGUI_FIELD_EDIT_START) {
		snprintf(context->editBuffer, editBufferSize, "%g", newValue);
	}
	else if (state == OMGUI_FIELD_EDIT_CANCELED) {
		context->editBuffer[0] = '\0';
	}

	if (newValue < minValue)
		newValue = minValue;
	if (newValue > maxValue)
		newValue = maxValue;

	return newValue;
}


int OmGuiIntField(OmGuiContext* context, int value, int minValue, int maxValue, unsigned char maxSize, unsigned char viewSize) {
	OmGui_Element* element = OmGui_AddElement(context, OMGUI_I32_FIELD, nullptr, nullptr);
	element->field.viewSize = viewSize;
	element->bufferDataOffset = 0;

	const u8 BUFFER_SIZE = sizeof(context->editBuffer);
	char buffer[sizeof(context->editBuffer)];

	char* editBuffer;
	u8 editBufferSize = maxSize + 1 < BUFFER_SIZE ? maxSize + 1 : BUFFER_SIZE;
	u8 size;

	if (!OmGui_TabElementIsActive(context, context->currentTab->index, element->index) || context->elementState != OMGUI_ELEMENT_STATE_EDITING) {
		editBuffer = buffer;
		size = snprintf(editBuffer, editBufferSize, "%d", value);
		if (size - 1 > editBufferSize)
			size = editBufferSize - 1;
	}
	else {
		editBuffer = context->editBuffer;
		size = (u8) strlen(context->editBuffer);
	}

	OmGui_FieldEditState state = OmGui_FieldUpdate(context, element, editBuffer, size, editBufferSize);

	i32 newValue = value;
	if (state == OMGUI_FIELD_EDIT_FINISHED) {
		newValue = (i32) strtol(context->editBuffer, nullptr, 10);
		context->editBuffer[0] = '\0';
	}
	else if (state == OMGUI_FIELD_EDIT_START) {
		snprintf(context->editBuffer, editBufferSize, "%d", newValue);
	}
	else if (state == OMGUI_FIELD_EDIT_CANCELED) {
		context->editBuffer[0] = '\0';
	}

	return OmGui_Clamp(newValue, minValue, maxValue);
}


void OmGuiLabel(OmGuiContext* context, const char* text) {
	OmGui_AddElement(context, OMGUI_LABEL, text, nullptr);
}

void OmGuiRow(OmGuiContext* context) {
	OmGui_AddElement(context, OMGUI_NEW_ROW, nullptr, nullptr);
}

void OmGuiCanvas(OmGuiContext* context, int width, int height, unsigned char id) {
	bool isNew;
	OmGui_Element* element = OmGui_AddElement(context, OMGUI_CANVAS, nullptr, &isNew);

	element->canvas.stretchW = height == 0;
	element->canvas.stretchH = width == 0;
	element->canvas.id = id;

	if (isNew || !element->canvas.stretchW) {
		element->rect.w = width < 0 ? 0 : width;
	}

	if (isNew || !element->canvas.stretchH) {
		element->rect.h = height < 0 ? 0 : height;
	}

	if (context->currentTab->isHidden)
		element->rect.w = 0;
}

void OmGuiTable(OmGuiContext* context, unsigned int columnsCount) {
	if (columnsCount == 0)
		return; 

	OmGui_Element* element = OmGui_AddElement(context, OMGUI_TABLE, nullptr, nullptr);

	element->bufferDataOffset = OmGui_PushBufferData(context, nullptr, sizeof(OmGui_TableColumn) * columnsCount);
	OmGui_TableColumn* columns = (OmGui_TableColumn*) OmGui_ElementBufferData(context, element);
	for (u32 i = 0; i < columnsCount; ++i)
		columns[i].width = 0;

	element->tableColumnsCount = columnsCount;
}

void OmGuiTableEnd(OmGuiContext* context) {
	OmGui_AddElement(context, OMGUI_TABLE_END, nullptr, nullptr);
}

int OmGuiSlider(OmGuiContext* context, int minValue, int maxValue, int value) {
	OmGui_Element* element = OmGui_AddElement(context, OMGUI_SLIDER, nullptr, nullptr);

	OmGui_Tab* tab = context->currentTab;

	if (element->rect.w == 0)
		element->rect.w = 1;

	if ((OmGui_ElementAllowInput(context, tab) && OmGui_PointInGuiRect(context->input.mouseX, context->input.mouseY, &element->rect)) || context->elementState == OMGUI_ELEMENT_STATE_SLIDING) {
		if (context->input.clickDown)
			context->elementState = OMGUI_ELEMENT_STATE_SLIDING;

		OmGui_TabElementSetHot(context, tab->index, element->index);
		OmGui_SetActive(context, tab->index, context->input.clickDown);
	}

	i32 clampedValue = OmGui_Clamp(value, minValue, maxValue);
	if (OmGui_TabElementIsHot(context, tab->index, element->index) && OmGui_TabIsActive(context, tab) && context->elementState == OMGUI_ELEMENT_STATE_SLIDING) {
		f32 ratio = (f32) OmGui_Clamp(context->input.mouseX - element->rect.x, 0, element->rect.w) / element->rect.w;
		clampedValue = minValue + (i32) roundf((maxValue - minValue) * ratio);

		OmGui_TabElementSetHot(context, tab->index, element->index);
		OmGui_SetActive(context, tab->index, true);

		if (context->input.clickUp) {
			context->activeElementIndex = INVALID_ARRAY_INDEX;
			context->elementState = OMGUI_ELEMENT_STATE_NORMAL;
		}
	}
	
	char buffer[16];
	snprintf(buffer, 16, "%d", clampedValue);
	element->bufferDataOffset = OmGui_PushBufferData(context, buffer, 16);

	f32 ratio = (f32)(clampedValue - minValue) / (maxValue - minValue);
	element->slider_value = (i32) roundf(element->rect.w * ratio);
	return clampedValue;
}

bool OmGuiSubTab(OmGuiContext* context, const char* text) {
	bool isNew;
	OmGui_Element* element = OmGui_AddElement(context, OMGUI_SUBTAB, text, &isNew);
	if (isNew)
		element->check_enabled = true;

	element->rect.y -= element->rect.h / 2;
	if (OmGui_ElementRectHot(context, context->currentTab, element->index, context->input.clickUp, &element->rect) && context->input.clickUp)
		element->check_enabled = !element->check_enabled;

	element->rect.y += element->rect.h / 2;
	return element->check_enabled;
}

bool OmGuiCheckbox(OmGuiContext* context, bool enabled) {
	OmGui_Element* element = OmGui_AddElement(context, OMGUI_CHECKBOX, nullptr, nullptr);
	element->check_enabled = enabled;

	if (OmGui_ElementRectHot(context, context->currentTab, element->index, context->input.clickUp, &element->rect) && context->input.clickUp)
		element->check_enabled = !element->check_enabled;

	return element->check_enabled;
}


void OmGui_TabRenderHeader(OmGuiContext* context, OmGui_Tab* tab, bool highlightHeader) {
	i32 borderColor = context->style.borderColor;
	i32 backColor = context->style.tabBackgroundColor;

	i32 bw = context->style.tabBorderWidth;
	i32 tabIndent = context->style.tabElementsIndent;

	borderColor = (context->tabState == OMGUI_TAB_STATE_RESIZING && OmGui_TabIsActive(context, tab)) || tab->side != OMGUI_NONE_FLAG ? context->style.borderResizingColor : context->style.borderColor;

	const i32 headerHeight = context->style.fontSize * 2;
	const i32 halfHeaderHeight = context->style.fontSize;

	i32 x = tab->rect.x;
	i32 y = tab->rect.y;

	OmGui_RectCommand(context, x - bw, y - bw, tab->rect.w + 2 * bw, tab->rect.h + 2 * bw, borderColor);
	OmGui_RectCommand(context, x, y, tab->rect.w, headerHeight, context->style.tabHeaderColor);

	if (highlightHeader)
		OmGui_RectCommand(context, x, y, tab->rect.w, halfHeaderHeight, context->style.tabHeaderHotColor);

	OmGui_RectCommand(context, x, y + headerHeight, tab->rect.w, tab->rect.h - headerHeight, backColor);
	OmGui_ScissorOnCommand(context, tab->rect.x, y, tab->rect.w, headerHeight);

	i32 offsetX = tabIndent + headerHeight / 4 + context->style.tabElementsSpacing;
	u32 nextIndex = tab->nextIndex;
	u32 currentChainIndex = 0;
	i32 maxWidth = 0;

	i32 tabWidth = OmGui_TabButtonWidth(context, tab);

	while (true) {
		if (currentChainIndex == tab->chainIndex) {
			OmGui_TabButtonRender(context, tab, tab);
			tab->button.offsetX = offsetX;
			offsetX += tabWidth + context->style.tabElementsSpacing;
			maxWidth = OmGui_Max(tabWidth, maxWidth); // this can be from last render
		}

		if (nextIndex == INVALID_ARRAY_INDEX)
			break;

		OmGui_Tab* nextTab = &context->tabs[nextIndex];
		nextIndex = nextTab->nextIndex;
		currentChainIndex++;
		if (offsetX < tab->rect.w) {
			OmGui_TabButtonRender(context, tab, nextTab);
		}
		nextTab->button.offsetX = offsetX;

		i32 width = OmGui_TabButtonWidth(context, nextTab);
		maxWidth = OmGui_Max(width, maxWidth); // this can be from last render
		offsetX += width + context->style.tabElementsSpacing;
	}

	if (tab->button.offsetX + tabWidth / 2 >= tab->rect.w && tab->chainIndex > 0 && context->tabState != OMGUI_TAB_STATE_UNDOCKING) {
		tab->chainIndex--;
	}
	//@Todo input here not nice (even though this is still header update)
	tab->dropDownMaxWidth = !context->input.clickUp && context->hotElementIndex == DROPDOWN_BUTTON_INDEX && (context->hotTabIndex == tab->index || tab->dropDownMaxWidth > 0) ? maxWidth : 0;

	OmGui_ScissorOffCommand(context);
}

static void OmGui_TabRender(OmGuiContext* context, OmGui_Tab* tab) {
	Assert(context->tabIDToTabHandle[tab->id] == tab->index);

	if (context->tabState != OMGUI_TAB_STATE_RESIZING || !OmGui_TabIsActive(context, tab))
		tab->side = OMGUI_NONE_FLAG;

	const i32 halfHeaderHeight = context->style.fontSize;
	const i32 headerHeight = 2 * context->style.fontSize;

	bool highlightHeader = false;

	if (OmGui_PointInGuiRect(context->input.mouseX, context->input.mouseY, &tab->rect) && !OmGui_TabIsActive(context, tab) && context->tabState == OMGUI_TAB_STATE_MOVING) {
		OmGui_TabElementSetHot(context, tab->index, INVALID_ARRAY_INDEX);
	}

	u32 index = tab->index;
	u32 tabsCount = 0;
	while (index != INVALID_ARRAY_INDEX) {
		OmGui_Tab* nextTab = &context->tabs[index];
		if (nextTab->button.offsetX < tab->rect.x + tab->rect.w)
			OmGui_TabButtonUpdate(context, tab, nextTab);

		index = nextTab->nextIndex;
		tabsCount += 1;
	}

	// @TODO this can be before tabs count will be hidden in 
	OmGui_TabDropDownUpdate(context, tab, tabsCount);

	OmGui_Rect rect = tab->rect;
	rect.h = halfHeaderHeight;

	if (OmGui_ElementRectHot(context, tab, INVALID_ARRAY_INDEX, context->input.clickDown, &rect)) {
		highlightHeader = true;
		if (context->input.clickDown)
			context->nextTabState = OMGUI_TAB_STATE_MOVING;
	}

	OmGui_Rect nextRect = tab->rect;
	if (context->tabState == OMGUI_TAB_STATE_MOVING && OmGui_TabIsActive(context, tab)) {
		nextRect.x += context->input.mouseX - context->input.oldMouseX;
		nextRect.y += context->input.mouseY - context->input.oldMouseY;
		highlightHeader = true;
		OmGui_SetActive(context, tab->index, true);
	}
	else if (!highlightHeader) {
		if (context->tabState == OMGUI_TAB_STATE_RESIZING && OmGui_TabIsActive(context, tab)) {
			OmGui_RectResize(context, &nextRect, tab->side, 100, 100, context->input.windowWidth, context->input.windowHeight);

			if (context->input.clickUp) {
				context->nextTabState = OMGUI_TAB_STATE_NORMAL;
			}
		}
		else if (OmGui_ElementAllowInput(context, tab)) {
			tab->side = OmGui_RectBorderCheck(context, &nextRect, context->style.borderCheckSize);

			if (tab->side != OMGUI_NONE_FLAG) {
				if (context->input.clickDown)
					context->nextTabState = OMGUI_TAB_STATE_RESIZING;

				context->cursor = OmGui_CursorFromBorder(tab->side);
				OmGui_SetActive(context, tab->index, context->input.clickDown);
			}
		}
	}

	OmGui_TabRenderHeader(context, tab, highlightHeader);

	i32 tabContentWidth = tab->rect.w;
	i32 futureTabContentWidth = nextRect.w;

	i32 scrollBarMinY = 0;
	if (OmGui_TabScrollbar(context, tab, &scrollBarMinY)) {
		tabContentWidth -= context->style.scrollbarWidth;
		futureTabContentWidth -= context->style.scrollbarWidth;
	}

	// @TODO we migth actualy want to cut button text... dont use scissor for that but calculate if possible
	OmGui_ScissorOnCommand(context, tab->rect.x, tab->rect.y + headerHeight, tabContentWidth, tab->rect.h - headerHeight - context->style.tabBorderWidth / 2);

	OmGui_Rect oldRect = tab->rect;

	tab->rect = nextRect;
	OmGui_TabContentRender(context, tab, tabContentWidth, futureTabContentWidth, scrollBarMinY);

	OmGui_ScissorOffCommand(context);

	{
		tab->rect = oldRect;
		OmGui_TabDropDownRender(context, tab, tabsCount);
		tab->rect = nextRect;
	}

	if (context->tabState == OMGUI_TAB_STATE_NORMAL && context->nextTabState == OMGUI_TAB_STATE_NORMAL && context->input.clickUp && OmGui_PointInGuiRect(context->input.mouseX, context->input.mouseY, &tab->rect)) {
		OmGui_SetActive(context, tab->index, true);
	}
}

static i32 OmGui_TableReport(OmGuiContext* context, const OmGui_Tab* tab, const OmGui_Element* table, const OmGui_Element** inOutIterator) {
	i32 contentY = 0;
	i32 maxY = 0;

	u32 currentColumn = 0;
	OmGui_TableColumn* columns = (OmGui_TableColumn*) OmGui_ElementBufferData(context, table);

	const OmGui_Element* element = *inOutIterator;
	const OmGui_Element* elementEnd = tab->elements.end();
	for (; element < elementEnd && element->type != OMGUI_TABLE_END; element++) {
		if (element->type == OMGUI_NEW_ROW) {
			contentY += maxY + context->style.tabElementsSpacing;
			maxY = 0;
			currentColumn = 0;
		}
		else if (element->type == OMGUI_TABLE || element->type == OMGUI_SUBTAB) {
			Assert(false);
			currentColumn++;
		}
		else {
			columns[currentColumn].width = OmGui_Max(element->rect.w, columns[currentColumn].width);
			maxY = OmGui_Max(element->rect.h, maxY);
			currentColumn++;
		}

		if (currentColumn == table->tableColumnsCount) {
			contentY += maxY + context->style.tabElementsSpacing;
			maxY = 0;
			currentColumn = 0;
		}
	}

	*inOutIterator = element;
	return contentY + maxY;
}

static i32 OmGui_TabContentReportHeight(OmGuiContext* context, OmGui_Tab* tab) {
	i32 contentY = 0;
	i32 maxY = 0;
	bool closed = false;

	if (tab->elements.count_ > 0 && tab->elements[0].type != OMGUI_SUBTAB) {
		contentY += 2 * context->style.tabElementsIndent;
	}

	const OmGui_Element* element = tab->elements.begin();
	const OmGui_Element* elementEnd = tab->elements.end();
	for (; element < elementEnd; element++) {
		if (closed && element->type != OMGUI_SUBTAB)
			continue;
		
		if (element->type == OMGUI_SUBTAB) {
			if (!closed)
				contentY += context->style.tabElementsIndent;

			contentY += maxY + element->rect.h;
			maxY = 0;
			closed = !element->check_enabled;

			if (element->check_enabled)
				contentY += context->style.tabElementsIndent;
		}
		else if (element->type == OMGUI_TABLE) {
			const OmGui_Element* table = element++;
			contentY += OmGui_TableReport(context, tab, table, &element);
			maxY = 0;
		}
		else if (element->type == OMGUI_TABLE_END) {
			Assert(false);
		}
		else if (element->type == OMGUI_NEW_ROW) {
			contentY += maxY + context->style.tabElementsSpacing;
			maxY = 0;
		}
		else {
			maxY = OmGui_Max(element->rect.h, maxY);
		}
	}

	return contentY + maxY;
}

static OmGui_Rect OmGui_TabDropDownRect(const OmGuiContext* context, const OmGui_Tab* tab) {
	const i32 halfHeaderHeight = context->style.fontSize;
	return OmGui_RectCreate(tab->rect.x + context->style.tabElementsIndent, tab->rect.y + halfHeaderHeight / 4 + halfHeaderHeight, halfHeaderHeight / 2, halfHeaderHeight / 2);
}

static bool OmGui_TabScrollbar(OmGuiContext* context, OmGui_Tab* tab, i32* outContentMinY) {
	i32 relativeX = context->input.mouseX - tab->rect.x;
	i32 relativeY = context->input.mouseY - tab->rect.y;
	i32 contentY = OmGui_TabContentReportHeight(context, tab);

	const i32 headerHeight = 2 * context->style.fontSize;
	i32 boxHeight = tab->rect.h - headerHeight;

	if (contentY <= boxHeight) {
		*outContentMinY = 0;
		return false;
	}

	i32 width = context->style.scrollbarWidth;
	i32 height = boxHeight * boxHeight / contentY;

	i32 y = tab->scrollbarOffset;

	i32 barColor = context->style.buttonColor;
	if ((context->tabState == OMGUI_TAB_STATE_SCROLLING && OmGui_TabIsActive(context, tab)) || 
	    (OmGui_ElementAllowInput(context, tab) && OmGui_PointInRect(relativeX, relativeY, tab->rect.w - width, headerHeight, width, boxHeight))) {

		bool setActive = false;
		barColor = context->style.buttonHotColor;
		if (context->input.clickDown) {
			setActive = true;
			y = relativeY - headerHeight - height / 2;
			
			context->nextTabState = OMGUI_TAB_STATE_SCROLLING;
		}
		else if (context->tabState == OMGUI_TAB_STATE_SCROLLING) {
			setActive = true;
			y += context->input.mouseY - context->input.oldMouseY;
		}
		OmGui_SetActive(context, tab->index, setActive);
	}

	if (context->tabState == OMGUI_TAB_STATE_SCROLLING && context->input.clickUp)
		context->nextTabState = OMGUI_TAB_STATE_NORMAL;

	y = OmGui_Clamp(y, 0, boxHeight - height);
	tab->scrollbarOffset = y;

	OmGui_RectCommand(context, tab->rect.x + tab->rect.w - width, tab->rect.y + headerHeight, width, boxHeight, context->style.tabBackgroundColor);
	OmGui_RectCommand(context, tab->rect.x + tab->rect.w - width + 4, tab->rect.y + y + headerHeight, width - 8, height, barColor);

	*outContentMinY = y * contentY / boxHeight;
	return true;

}


static void OmGui_TabContentRender(OmGuiContext* context, OmGui_Tab* tab, i32 tabContentWidth, i32 futureTabContentWidth, i32 offsetY) {
	const i32 headerHeight = 2 * context->style.fontSize;

	i32 tabIndent = context->style.tabElementsIndent;
	i32 tabSpacing = context->style.tabElementsSpacing;
	i32 labelIndent = context->style.labelIndent;

	i32 realTabWidth = tabContentWidth;
	i32 realTabHeight = tab->rect.h;

	i32 x = tab->rect.x + tabIndent;
	i32 y = tab->rect.y + tabIndent + headerHeight - offsetY;

	i32 maxY = 0;

	bool closed = false;
	if (tab->elements.count_ > 0) {
		OmGui_Element* first = &tab->elements[0];
		if (first->type == OMGUI_SUBTAB) {
			y -= 2 * context->style.tabElementsIndent; // one for subtab one for start
		}
	}

	i32 basicHeight = OmGui_TabReportRectH(context);

	u32 columnsCount = 0;
	u32 currentColumn = 0;
	u32 currentColumnWidth = 0;
	OmGui_TableColumn* tableColumns = nullptr;

	for (auto& element : tab->elements) {
		if (closed && element.type != OMGUI_SUBTAB) {
			element.rect.x = -9000;
			element.rect.y = -9000;
			continue;
		}

		switch (element.type) {
		case OMGUI_TABLE:
			if (!tableColumns) {
				tableColumns = (OmGui_TableColumn*) OmGui_ElementBufferData(context, &element);
				currentColumn = 0;
				columnsCount = element.tableColumnsCount;
				currentColumnWidth = tableColumns[currentColumn].width;
			}
			break;
		case OMGUI_TABLE_END:
		{
			// the same as new row
			y += maxY;
			x = tab->rect.x + tabIndent;
			maxY = 0;

			tableColumns = nullptr;
			columnsCount = 0;
			currentColumnWidth = 0;

			break;
		}
		case OMGUI_I32_FIELD:
		case OMGUI_F32_FIELD:
		case OMGUI_TEXT_FIELD:
		{
			OmGui_TextFieldRender(context, tab, &element);
			element.rect = OmGui_RectCreate(x, y, OmGui_TabReportCenteredRectW(context, element.field.viewSize), basicHeight + context->style.labelIndent * 2);
			x += OmGui_Max(element.rect.w, currentColumnWidth) + tabSpacing;
			maxY = OmGui_Max(element.rect.h, maxY);
			break;
		}
		case OMGUI_LABEL:
		{
			OmGui_LabelRender(context, &element.rect, element.text);
			element.rect = OmGui_RectCreate(x, y, OmGui_TabReportRectW(context, (i32) strlen(element.text)), basicHeight + context->style.labelIndent * 2);
			x += OmGui_Max(element.rect.w, currentColumnWidth) + tabSpacing;
			maxY = OmGui_Max(element.rect.h, maxY);
			break;
		}
		case OMGUI_BUTTON:
		{
			OmGui_ButtonRender(context, tab, &element);
			element.rect = OmGui_RectCreate(x, y, OmGui_TabReportCenteredRectW(context, (i32) strlen(element.text)), basicHeight + context->style.labelIndent * 2);
			x += OmGui_Max(element.rect.w, currentColumnWidth) + tabSpacing;
			maxY = OmGui_Max(element.rect.h, maxY);
			break;
		}
		case OMGUI_CHECKBOX:
		{
			OmGui_CheckBoxRender(context, tab, &element);
			element.rect = OmGui_RectCreate(x, y + context->style.labelIndent, basicHeight, basicHeight);
			x += OmGui_Max(element.rect.w, currentColumnWidth) + tabSpacing;
			maxY = OmGui_Max(element.rect.h, maxY);
			break;
		}
		case OMGUI_CANVAS:
		{
			OmGui_UserCanvasCommand(context, element.rect.x, element.rect.y, element.rect.w, element.rect.h, element.canvas.id);

			i32 w = element.canvas.stretchW ? tab->rect.x + futureTabContentWidth - x - tabIndent : element.rect.w;
			// Y is absolute fix with tab->rect.y .... Y containts offsetY fix...
			i32 h = element.canvas.stretchH ? tab->rect.y + realTabHeight - y - offsetY - tabIndent : element.rect.h;
			element.rect = OmGui_RectCreate(x, y, w < 0 ? 0 : w, h < 0 ? 0 : h);

			x += OmGui_Max(element.rect.w, currentColumnWidth) + tabSpacing;
			maxY = OmGui_Max(element.rect.h, maxY);
			break;
		}
		case OMGUI_NEW_ROW:
		{
			y += maxY + tabSpacing;
			x = tab->rect.x + tabIndent;
			maxY = 0;
			break;
		}
		case OMGUI_SLIDER: 
		{
			if (element.rect.w > 0)
				OmGui_SliderRender(context, tab, &element);
			element.rect = OmGui_RectCreate(x, y, futureTabContentWidth - x + tab->rect.x - tabIndent, OmGui_TabReportRectH(context));
			x += OmGui_Max(element.rect.w, currentColumnWidth) + tabSpacing;
			maxY = OmGui_Max(element.rect.h, maxY);
			break;
		}
		case OMGUI_SUBTAB:
		{
			if (tableColumns) // invalid -> assert is in table report
				break;

			y += maxY;
			if (!closed)
				y += tabIndent;

			i32 rh = OmGui_SubTabReportH(context);
			i32 yy = element.rect.y - context->style._signPrimarySize / 2;

			OmGui_RectCommand(context, element.rect.x - tabIndent, yy+1, realTabWidth, rh-1, context->style.tabBackgroundColor);
			OmGui_CStringCommand(context, element.text, element.rect.x + context->style._signSecondarySize + context->style.tabElementsIndent + context->style.labelIndent, yy, context->style.textColor);
			OmGui_ToggleButtonRender(context, tab, &element);
			x = tab->rect.x + tabIndent;
			element.rect = OmGui_RectCreate(x, y + context->style._signPrimarySize / 2, context->style._signSecondarySize, rh);

			y += rh;
			closed = !element.check_enabled;

			if (element.check_enabled)
				y += tabIndent;

			maxY = 0;
			break;
		}
		default:
			Assert(false);
			break;
		}

		if (tableColumns && element.type != OMGUI_TABLE) {
			if (currentColumn == columnsCount - 1) {
				currentColumn = 0;
				y += maxY + tabSpacing;
				x = tab->rect.x + tabIndent;
				maxY = 0;
			}
			else {
				currentColumn = element.type == OMGUI_NEW_ROW ? 0 : currentColumn + 1;
			}

			currentColumnWidth = tableColumns[currentColumn].width;
		}
	}
}

char* OmGuiUpdate(OmGuiContext* context, unsigned int* outBufferSize, OmGuiCursorType* optOutCursor) {
	if (context->tabs.count_ == 0) {
		*outBufferSize = 0;
		if (optOutCursor) 
			*optOutCursor = OMGUI_CURSOR_NORMAL;

		context->renderDataSize = 0;
		context->bufferDataSize = 0;
		return nullptr;
	}

	if (context->currentTab && (u32) context->currentElement < context->currentTab->elements.count_) {
		context->currentTab->elements.count_ = context->currentElement;
	}

	context->hasOverlay = false;

	// Set hot tab -> the last rendered with mouse inside is hot
	if (context->tabState == OMGUI_TAB_STATE_NORMAL) {
		OmGui_Tab* tab = context->tabs._data;
		OmGui_Tab* tabEnd = tab + context->tabs.count_;
		OmGui_Rect rect;
		for (; tab < tabEnd; tab++) {
			if (!tab->isHidden) {
				rect = tab->rect;
				rect.x -= context->style.borderCheckSize;
				rect.y -= context->style.borderCheckSize;
				rect.w += context->style.borderCheckSize * 2;
				rect.h += context->style.borderCheckSize * 2;

				if (context->hotTabIndex != tab->index && OmGui_PointInGuiRect(context->input.mouseX, context->input.mouseY, &rect)) {
					context->hotTabIndex = tab->index;
				}
			}
		}
	}
	else {
		OmGui_TabElementSetHot(context, INVALID_ARRAY_INDEX, INVALID_ARRAY_INDEX);
	}

	// Main logic for each tab
	for (u32 i = 0; i < context->tabs.count_; ++i) {
		if (!context->tabs[i].isHidden)
			OmGui_TabRender(context, &context->tabs[i]);
	}

	// Logic for the active tab
	if (context->tabState == OMGUI_TAB_STATE_NORMAL) {
		if (context->nextActiveTabIndex != INVALID_ARRAY_INDEX &&  !context->tabs[context->nextActiveTabIndex].isHidden) {
			OmGui_TabSetActive(context, context->nextActiveTabIndex);
		}
		if (context->nextActiveTabIndex != context->hotTabIndex && context->nextTabState == OMGUI_TAB_STATE_UNDOCKING) {
			Assert(context->tabState != OMGUI_TAB_STATE_UNDOCKING && context->nextTabState == OMGUI_TAB_STATE_UNDOCKING);
			Assert(context->nextActiveTabIndex != context->hotTabIndex);

			OmGui_Tab* tab = &context->tabs[context->tabs.count_ - 1];

			u32 chainIndex = 1;
			u32 nextIndex = tab->nextIndex;
			while (nextIndex != context->hotTabIndex) {
				Assert(nextIndex != INVALID_ARRAY_INDEX);
				chainIndex++;
				nextIndex = context->tabs[nextIndex].nextIndex;
			}

			if (chainIndex <= tab->chainIndex)
				chainIndex--;

			u32 currentChainIndex = tab->chainIndex;
			i32 index = tab->index;
			OmGui_HiddenTabSetActive(context, tab->index, context->hotTabIndex, chainIndex);
			OmGui_HiddenTabUndock(context, tab->rect, context->hotTabIndex);
			context->tabs[context->tabs.count_ - 1].isHidden = false;

			if (currentChainIndex > chainIndex)
				currentChainIndex -= 1;

			OmGui_TabDock(context, &context->tabs[context->tabs.count_ - 1], context->hotTabIndex, currentChainIndex);

			if (context->input.clickUp)
				context->nextTabState = OMGUI_TAB_STATE_NORMAL;
		}
	}
	else if (context->tabState == OMGUI_TAB_STATE_MOVING) {
		i32 size = context->style.fontSize * 2;
		u32 colors[2] = {context->style.buttonColor, context->style.buttonColor};
		OmGui_Rect baseRects[2] = {OmGui_RectCreate(context->input.windowWidth / 2 - size / 2, 0, size, size),
			                      OmGui_RectCreate(0, context->input.windowHeight / 2 - size / 2, size, size)};

		OmGui_Rect setRects[2] = {OmGui_RectCreate(0, 0, context->input.windowWidth, context->input.windowHeight),
			                      OmGui_RectCreate(0, 0, context->input.windowWidth / 2, context->input.windowHeight)};

		for (u32 i = 0; i < 2; ++i) {
			if (OmGui_PointInGuiRect(context->input.mouseX, context->input.mouseY, &baseRects[i])) {
				context->hotTabIndex = INVALID_ARRAY_INDEX;
				colors[i] = context->style.buttonHotColor;
				if (context->input.clickUp)
					context->tabs[context->tabs.count_ - 1].rect = setRects[i];
				break;
			}
		}

		if (context->hotTabIndex != INVALID_ARRAY_INDEX && context->hotElementIndex == INVALID_ARRAY_INDEX && context->tabs[context->tabs.count_ - 1].nextIndex == INVALID_ARRAY_INDEX) {
			Assert(!context->tabs[context->hotTabIndex].isHidden);
			OmGui_Tab* dockTab = &context->tabs[context->hotTabIndex];
			OmGui_Tab* tab = &context->tabs[context->tabs.count_ - 1];

			if (context->input.clickUp) {
				// This is place where docking of a chain instead of single tab can be done, but so far no solution except n^2
				i32 dockTabId = dockTab->id;
				i32 tabId = tab->id;

				OmGui_TabSetActive(context, dockTab->index); 

				// pointers were changed
				OmGui_Tab* activeTab = &context->tabs[context->tabIDToTabHandle[dockTabId]];
				u32 tabIndex = context->tabs[context->tabIDToTabHandle[tabId]].index;

				OmGui_TabDock(context, activeTab, tabIndex, 0);
				OmGui_HiddenTabSetActive(context, activeTab->index, tabIndex, 0);
			}
			else {
				i32 bw = context->style.tabBorderWidth;
				OmGui_RectCommand(context, dockTab->rect.x - bw, dockTab->rect.y - bw, dockTab->rect.w + 2 * bw, dockTab->rect.h + 2 * bw, context->style.borderColor);
				OmGui_RectCommand(context, dockTab->rect.x, dockTab->rect.y, dockTab->rect.w, dockTab->rect.h, context->style.tabHeaderHotColor);
			}
		}
		for (u32 i = 0; i < 2; ++i) {
			OmGui_RectCommand(context, baseRects[i].x, baseRects[i].y, baseRects[i].w, baseRects[i].h, colors[i]);
		}

		if (context->input.clickUp) {
			context->nextTabState = OMGUI_TAB_STATE_NORMAL;
		}
	}
	else if (context->tabState == OMGUI_TAB_STATE_UNDOCKING) {
		OmGui_Tab* activeTab = &context->tabs[context->tabs.count_ - 1];
		OmGui_Rect rect = activeTab->rect;

		Assert(activeTab->nextIndex != INVALID_ARRAY_INDEX);

		i32 activeIndex = activeTab->index;
		u32 hiddenIndex = activeTab->nextIndex;

		if (!OmGui_PointInRect(context->input.mouseX, context->input.mouseY, activeTab->rect.x + activeTab->button.offsetX, activeTab->rect.y + context->style.fontSize, 
			OmGui_TabButtonWidth(context, activeTab), context->style.fontSize)) {
			rect.x += context->input.mouseX - context->input.oldMouseX;
			rect.y += context->input.mouseY - context->input.oldMouseY;

			OmGui_HiddenTabSetActive(context, activeTab->index, hiddenIndex, 0);
			OmGui_HiddenTabUndock(context, rect, hiddenIndex);
			OmGui_TabSetActive(context, hiddenIndex);

			context->nextTabState = OMGUI_TAB_STATE_MOVING;
		}

		if (context->input.clickUp) {
			context->nextTabState = OMGUI_TAB_STATE_NORMAL;
		}
	}

	// Cursor
	if (context->hotTabIndex != INVALID_ARRAY_INDEX) {
		if (context->hotElementIndex == INVALID_ARRAY_INDEX) {
			if (context->tabState == OMGUI_TAB_STATE_RESIZING) {
				context->cursor = OmGui_CursorFromBorder(context->tabs[context->hotTabIndex].side);
			}
		}
		else if (context->hotElementIndex == DROPDOWN_BUTTON_INDEX || context->hotElementIndex == TAB_BUTTON_INDEX) {
			context->cursor = OMGUI_CURSOR_POINTER;
		}
		else {
			const OmGui_Element* element = &context->tabs[context->hotTabIndex].elements[context->hotElementIndex];

			switch (element->type) {
			case OMGUI_TEXT_FIELD:
			case OMGUI_U32_FIELD:
			case OMGUI_I32_FIELD:
			case OMGUI_F32_FIELD:
				context->cursor = OMGUI_CURSOR_IBEAM;
				break;
			case OMGUI_BUTTON:
			case OMGUI_SUBTAB:
			case OMGUI_DROPDOWN:
			case OMGUI_SLIDER:
			case OMGUI_CHECKBOX:
				context->cursor = OMGUI_CURSOR_POINTER;
				break;
			}
		}
	}

	OmGui_TabElementSetHot(context, context->hotTabIndex, INVALID_ARRAY_INDEX);
	context->nextActiveTabIndex = INVALID_ARRAY_INDEX;


	context->tabState = context->nextTabState;

	if (optOutCursor)
		*optOutCursor = context->cursor;

	*outBufferSize = context->renderDataSize;

	context->cursor = OMGUI_CURSOR_NORMAL;

	context->input.oldMouseX = context->input.mouseX;
	context->input.oldMouseY = context->input.mouseY;

	context->renderDataSize = 0; // User is still using it but next updated it will be used from scratch
	context->bufferDataSize = 0; // User is still using it but next updated it will be used from scratch
	return context->renderData;
}



static u32 OmGui_Hash(const char* text) {
	i32 i = 0;
	u32 hash = 0x13371337; // @TODO
	while (text[i]) {
		u32 h = (u32) text[i];
		hash = (hash * h) ^ hash;
		++i;
	}
	return hash;
}

static i32 OmGui_Clamp(i32 value, i32 min, i32 max) {
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

static void OmGui_Swap(u32* a, u32* b) {
	u32 tmp = *a;
	*a = *b;
	*b = tmp;
}

static i32 OmGui_Max(i32 a, i32 b) {
	return a > b ? a : b;
}

static bool OmGui_PointInRect(i32 px, i32 py, i32 rx, i32 ry, i32 w, i32 h) {
	bool in = (rx <= px) && (px <= rx + w);
	bool in2 = (ry <= py) && (py <= ry + h);
	return in && in2;
}

static bool OmGui_PointInGuiRect(i32 px, i32 py, const OmGui_Rect* rect) {
	bool in = (rect->x <= px) && (px <= rect->x + rect->w);
	bool in2 = (rect->y <= py) && (py <= rect->y + rect->h);
	return in && in2;
}

static i32 OmGui_TabReportRectW(OmGuiContext* context, u32 charCount) {
	return (i32) charCount * context->style._fontSizeW;
}

static i32 OmGui_TabReportCenteredRectW(OmGuiContext* context, u32 charCount) {
	return OmGui_TabReportRectW(context, charCount) + context->style.labelIndent * 2;
}

static i32 OmGui_TabReportRectH(OmGuiContext* context) {
	return context->style.fontSize;
}

static i32 OmGui_SubTabReportH(OmGuiContext* context) {
	return context->style.fontSize;
}

static OmGui_BorderFlags OmGui_RectBorderCheck(OmGuiContext* context, OmGui_Rect* rect, i32 borderLimit) {
	i32 side = OMGUI_NONE_FLAG;

	i32 relativeX = context->input.mouseX - rect->x;
	i32 relativeY = context->input.mouseY - rect->y;

	bool xBetween = (-borderLimit <= relativeX) && (relativeX <= rect->w + borderLimit);
	bool yBetween = (-borderLimit <= relativeY) && (relativeY <= rect->h + borderLimit);

	if (abs(rect->w - relativeX) < borderLimit && yBetween)
		side |= OMGUI_RIGHT_FLAG;

	if (abs(relativeX) < borderLimit && yBetween)
		side |= OMGUI_LEFT_FLAG;

	if (abs(rect->h - relativeY) < borderLimit && xBetween)
		side |= OMGUI_DOWN_FLAG;

	if (abs(relativeY) < borderLimit && xBetween)
		side |= OMGUI_UP_FLAG;

	return (OmGui_BorderFlags) side;
}

static void OmGui_RectResize(OmGuiContext* context, OmGui_Rect* rect, OmGui_BorderFlags side, i32 minW, i32 minH, i32 maxW, i32 maxH) {
	i32 deltaX = context->input.mouseX - context->input.oldMouseX;
	i32 deltaY = context->input.mouseY - context->input.oldMouseY;

	i32 w = rect->w;
	i32 h = rect->h;

	if (side & OMGUI_LEFT_FLAG) {
		rect->w += -deltaX;
	}
	else if (side & OMGUI_RIGHT_FLAG) {
		rect->w += deltaX;
	}

	if (side & OMGUI_UP_FLAG) {
		rect->h += -deltaY;
	}
	else if (side & OMGUI_DOWN_FLAG) {
		rect->h += deltaY;
	}

	rect->w = OmGui_Clamp(rect->w, minW, maxW);
	rect->h = OmGui_Clamp(rect->h, minH, maxH);

	if (side & OMGUI_LEFT_FLAG)
		rect->x += w - rect->w;

	if (side & OMGUI_UP_FLAG)
		rect->y += h - rect->h;
}

static void OmGui_TabDropDownUpdate(OmGuiContext* context, OmGui_Tab* tab, u32 tabsCount) {
	i32 index = -1;
	if (context->tabState == OMGUI_TAB_STATE_NORMAL && (OmGui_TabIsActive(context, tab) || !context->activeTabHasCursor)) {
		i32 h = OmGui_TabReportRectH(context);
		OmGui_Rect rect = OmGui_TabDropDownRect(context, tab);

		if (tab->dropDownMaxWidth > 0 && OmGui_PointInRect(context->input.mouseX, context->input.mouseY, rect.x, rect.y + rect.h, tab->dropDownMaxWidth, (i32) tabsCount * h)) {
			index = (context->input.mouseY - (rect.y + rect.h)) / h; 
			OmGui_TabElementSetHot(context, tab->index, DROPDOWN_BUTTON_INDEX);
			context->hasOverlay = true;
		}
		else if (OmGui_PointInGuiRect(context->input.mouseX, context->input.mouseY, &rect)) {
			if (tab->nextIndex != INVALID_ARRAY_INDEX) {
				OmGui_TabElementSetHot(context, tab->index, DROPDOWN_BUTTON_INDEX);
				context->hasOverlay = true;
			}
		}
	}

	if (index > 0) {
		OmGui_Tab* nextTab = &context->tabs[tab->nextIndex];
		i32 i = 1;
		while (i++ < index && nextTab->nextIndex != INVALID_ARRAY_INDEX)
			nextTab = &context->tabs[nextTab->nextIndex];

		OmGui_TabElementSetHot(context, nextTab->index, DROPDOWN_BUTTON_INDEX);
		OmGui_SetActive(context, tab->index, context->input.clickUp);

		if (context->input.clickUp)
			context->nextTabState = OMGUI_TAB_STATE_UNDOCKING;  //@TODO this undocking is bad or just move the undocking from nexttabstate to current state
	}
}

static void OmGui_TabButtonUpdate(OmGuiContext* context, OmGui_Tab* baseTab, OmGui_Tab* buttonTab) {
	i32 h = OmGui_TabReportRectH(context);

	if (OmGui_ElementAllowInput(context, baseTab) && 
		OmGui_PointInRect(context->input.mouseX, context->input.mouseY, baseTab->rect.x + buttonTab->button.offsetX, baseTab->rect.y + h, OmGui_TabButtonWidth(context, buttonTab), h)) {
		OmGui_TabElementSetHot(context, buttonTab->index, TAB_BUTTON_INDEX);
		OmGui_SetActive(context, baseTab->index, context->input.clickDown);

		if (context->input.clickDown) {
			context->nextTabState = baseTab->nextIndex != INVALID_ARRAY_INDEX ? OMGUI_TAB_STATE_UNDOCKING : OMGUI_TAB_STATE_MOVING;
		}
	}
}

OmGui_FieldEditState OmGui_FieldUpdate(OmGuiContext* context, OmGui_Element* element, char* buffer, u8 size, u8 maxSize) {
	OmGui_FieldEditState state = OMGUI_FIELD_NO_EDIT;

	if (OmGui_TabElementIsActive(context, context->currentTab->index, element->index) && context->elementState == OMGUI_ELEMENT_STATE_EDITING) {
		bool hasDot = false;
		bool allowNumeric = element->type == OMGUI_F32_FIELD || element->type == OMGUI_I32_FIELD;
		bool allowMinus = element->type == OMGUI_F32_FIELD || element->type == OMGUI_I32_FIELD;
		bool allowDot = element->type == OMGUI_F32_FIELD;
		bool allowText = element->type == OMGUI_TEXT_FIELD;

		size = (u8) strlen(buffer);
		state = OMGUI_FIELD_EDITING;

		if (allowDot) {
			for (u32 i = 0; i < size; ++i) {
				if (buffer[i] == '.') {
					hasDot = true;
					break;
				}
			}
		}
		const char* inputData = context->bufferData + context->input.bufferInput;
		for (u32 i = 0; i < context->input.inputCount; ++i) {
			u8 c = inputData[i];
			if (c == KEY_TYPE_BACKSPACE) {
				if (size > 0) {
					size--;
					if (allowDot && buffer[size] == '.')
						hasDot = false;
				}
			}
			else if (c == KEY_TYPE_ENTER) {
				state = OMGUI_FIELD_EDIT_FINISHED;
				break;
			}
			else if (c == KEY_TYPE_TAB) {
				context->rotateActive = true;
				state = OMGUI_FIELD_EDIT_FINISHED;
				break;
			}
			else if (allowDot && c == '.' && !hasDot) {
				buffer[size++] = c;
			}
			else if (allowMinus && c == '-' && size == 0) {
				buffer[size++] = c;
			}
			else if (allowNumeric && '0' <= c && c <= '9' && size < maxSize - 1) {
				buffer[size++] = c;
			}
			else if (allowText && c < 128 && size < maxSize - 1) {
				buffer[size++] = c;
			}
		}
		buffer[size] = '\0';
	}

	if (context->rotateActive && OmGui_TabIsActive(context, context->currentTab) && context->activeElementIndex != element->index) {
		context->activeElementIndex = element->index;
		context->rotateActive = false;
		context->elementState = OMGUI_ELEMENT_STATE_EDITING;
		state = OMGUI_FIELD_EDIT_START;
	}

	if (state == OMGUI_FIELD_EDITING) {
		if (context->input.clickUp) { // @TODO clickdown
			state = OMGUI_FIELD_EDIT_CANCELED;
			context->rotateActive = false;
		}
		else { // has to be reset
			context->activeElementIndex = element->index;
			context->elementState = OMGUI_ELEMENT_STATE_EDITING;
		}
	}

	if (state == OMGUI_FIELD_EDIT_FINISHED || state == OMGUI_FIELD_EDIT_CANCELED) {
		context->activeElementIndex = INVALID_ARRAY_INDEX;
		context->elementState = OMGUI_ELEMENT_STATE_NORMAL;
	}

	u8 viewSize = element->field.viewSize;
	{
		size++; // null terminator
		viewSize++;
		u32 drawSize = size;
		element->field.viewOffset = 0;
		if (size > viewSize) {
			element->field.viewOffset = size - viewSize;
			drawSize = viewSize;
		}
		i32 lastIndex = drawSize - 1 + element->field.viewOffset;
		char last = buffer[lastIndex];
		buffer[lastIndex] = '\0'; // if the string is longer than viewSize, null terminated string is needed
		element->bufferDataOffset = OmGui_PushBufferData(context, buffer + element->field.viewOffset, drawSize);
		buffer[lastIndex] = last;
	}

	if (state != OMGUI_FIELD_EDIT_CANCELED && OmGui_ElementRectHot(context, context->currentTab, element->index, context->input.clickUp, &element->rect) && context->input.clickUp) {
		context->activeElementIndex = element->index;
		context->elementState = OMGUI_ELEMENT_STATE_EDITING;
		state = OMGUI_FIELD_EDIT_START;
	}

	return state;
}

static void OmGui_TabDropDownRender(OmGuiContext* context, const OmGui_Tab* tab, u32 tabsCount) {
	OmGui_Rect rect = OmGui_TabDropDownRect(context, tab);

	OmGui_RectCommand(context, rect.x, rect.y, rect.w, rect.h, context->style.buttonColor);

	if (tab->dropDownMaxWidth> 0) {
		i32 x = rect.x;
		i32 offsetY = rect.y + rect.h;

		i32 rowHeight = OmGui_TabReportRectH(context);
		i32 bw = context->style.elementBorderWidth;

		OmGui_RectCommand(context, x - bw, offsetY - bw, tab->dropDownMaxWidth + bw * 2, tabsCount * rowHeight + bw * 2, context->style.textColor); // border
		OmGui_RectCommand(context, x, offsetY, tab->dropDownMaxWidth, tabsCount * rowHeight, context->style.buttonColor); // background

		i32 index = tab->index;
		while (index != INVALID_ARRAY_INDEX) {
			OmGui_Tab* nextTab = &context->tabs[index];
			if (DROPDOWN_BUTTON_INDEX == context->hotElementIndex && nextTab->index == context->hotTabIndex)
				OmGui_RectCommand(context, x, offsetY, tab->dropDownMaxWidth, rowHeight, context->style.buttonHotColor);

			OmGui_CStringCommand(context, nextTab->button.text, x+ context->style.labelIndent, offsetY, context->style.textColor);
			index = nextTab->nextIndex;
			offsetY += rowHeight;
		}
	}
}

static i32 OmGui_TabButtonWidth(OmGuiContext* context, OmGui_Tab* buttonTab) {
	return OmGui_TabReportCenteredRectW(context, buttonTab->button.textSize);
}

static void OmGui_TabButtonRender(OmGuiContext* context, OmGui_Tab* baseTab, OmGui_Tab* buttonTab) {
	bool hover = OmGui_TabElementIsHot(context, buttonTab->index, TAB_BUTTON_INDEX);
	i32 color = hover ? context->style.buttonHotColor : context->style.buttonColor;

	if (!buttonTab->isHidden)
		color = context->style.tabBackgroundColor;

	OmGui_RectCommand(context, baseTab->rect.x + buttonTab->button.offsetX, baseTab->rect.y + context->style.fontSize, OmGui_TabButtonWidth(context, buttonTab), OmGui_TabReportRectH(context), color);
	OmGui_CStringCommand(context, buttonTab->button.text, baseTab->rect.x + buttonTab->button.offsetX + context->style.labelIndent,
		baseTab->rect.y + context->style.fontSize, context->style.textColor);
}

static void OmGui_LabelRender(OmGuiContext* context, const OmGui_Rect* rect, const char* text) {
	OmGui_CStringCommand(context, text, rect->x, rect->y + context->style.labelIndent, context->style.textColor);
}

static void OmGui_ButtonRender(OmGuiContext* context, const OmGui_Tab* tab, const OmGui_Element* element) {
	bool hover = OmGui_TabElementIsHot(context, tab->index, element->index);
	i32 color = hover ? context->style.buttonHotColor : context->style.buttonColor;

	OmGui_RectCommand(context, element->rect.x, element->rect.y, element->rect.w, element->rect.h, color);
	OmGui_CStringCommand(context, element->text, element->rect.x + context->style.labelIndent, element->rect.y + context->style.labelIndent, context->style.textColor);
}

static void OmGui_ToggleButtonRender(OmGuiContext* context, const OmGui_Tab* tab, const OmGui_Element* element) {
	bool hover = OmGui_TabElementIsHot(context, tab->index, element->index);

	i32 halfSize = element->rect.w / 2;

	i32 color = hover ? context->style.buttonHotColor : context->style.buttonColor;
	if (element->check_enabled) {
		OmGui_TriangleCommand(context, element->rect.x + halfSize, element->rect.y - halfSize / 2, element->rect.w, 180, color);
	}
	else {
		OmGui_TriangleCommand(context, element->rect.x, element->rect.y, element->rect.w, -90, color);
	}
}

static void OmGui_TextFieldRender(OmGuiContext* context, const OmGui_Tab* tab, const OmGui_Element* element) {
	bool hot = OmGui_TabElementIsHot(context, tab->index, element->index);
	i32 color = hot ? context->style.textFieldHighlightColor : context->style.tabBackgroundColor;

	if (OmGui_TabElementIsActive(context, tab->index, element->index)) {
		color = context->style.textFieldHighlightColor;
	}

	const char* text = OmGui_ElementBufferData(context, element);

	i32 bw = context->style.elementBorderWidth;
	i32 bw2 = 2 * context->style.elementBorderWidth;
	OmGui_RectCommand(context, element->rect.x, element->rect.y, element->rect.w, element->rect.h, context->style.borderColor);
	OmGui_RectCommand(context, element->rect.x + bw, element->rect.y + bw, element->rect.w - bw2, element->rect.h - bw2, color);
	OmGui_CStringCommand(context, text, element->rect.x + context->style.labelIndent, element->rect.y + context->style.labelIndent, context->style.textColor);

	if (context->activeElementIndex == element->index && context->elementState == OMGUI_ELEMENT_STATE_EDITING) {
		i32 w = OmGui_TabReportCenteredRectW(context, (u32) strlen(text));
		i32 h = OmGui_TabReportRectH(context);
		OmGui_RectCommand(context, element->rect.x + w - bw2, element->rect.y + element->rect.h / 2 - h / 2, bw, h, 255);
	}
}

void OmGui_CheckBoxRender(OmGuiContext* context, const OmGui_Tab* tab, const OmGui_Element* element) {
	bool hover = OmGui_TabElementIsHot(context, tab->index, element->index);

	i32 color = hover ? context->style.buttonHotColor : context->style.buttonColor;

	i32 bw = context->style.elementBorderWidth;
	i32 bw2 = 2 * context->style.elementBorderWidth;
	i32 bw4 = 4 * context->style.elementBorderWidth;

	OmGui_RectCommand(context, element->rect.x, element->rect.y, element->rect.w, element->rect.h, color);
	OmGui_RectCommand(context, element->rect.x + bw, element->rect.y + bw, element->rect.w - bw2, element->rect.h - bw2, context->style.tabBackgroundColor);

	if (element->check_enabled) {
		OmGui_RectCommand(context, element->rect.x + bw2, element->rect.y + bw2, element->rect.w - bw4, element->rect.h - bw4, color);
	}
}

void OmGui_SliderRender(OmGuiContext* context, const OmGui_Tab* tab, const OmGui_Element* element) {
	bool hover = OmGui_TabElementIsHot(context, tab->index, element->index);
	i32 color = hover ? context->style.buttonHotColor : context->style.buttonColor;

	OmGui_RectCommand(context, element->rect.x, element->rect.y + context->style.fontSize / 2 - context->style.fontSize / 8, element->rect.w, context->style.fontSize / 4, context->style.buttonColor);
	OmGui_RectCommand(context, element->rect.x + element->slider_value - context->style.fontSize / 2, element->rect.y, context->style.fontSize, element->rect.h, color);

	if (hover && element->text) {
		const char* text = OmGui_ElementBufferData(context, element);

		i32 rectWidth = OmGui_TabReportCenteredRectW(context, (i32) strlen(text));
		i32 rectHeight = OmGui_TabReportRectH(context);

		i32 x = element->rect.x + element->slider_value;
		i32 y = element->rect.y;

		x += element->slider_value > element->rect.w / 2 ? -(rectWidth + context->style.fontSize) : rectWidth ;

		OmGui_RectCommand(context, x, y, rectWidth, rectHeight, color);
		OmGui_CStringCommand(context, text, x + context->style.labelIndent, y, context->style.textColor);
	}
}

static void OmGui_SetActive(OmGuiContext* context, u32 tabIndex, bool setActive) {
	if (setActive) {
		context->nextActiveTabIndex = tabIndex;
	}
}

OmGuiCursorType OmGui_CursorFromBorder(OmGui_BorderFlags border) {
	OmGuiCursorType cursor = OMGUI_CURSOR_NORMAL;
	if (border & OMGUI_UP_FLAG) {
		if (border & OMGUI_RIGHT_FLAG) {
			cursor = OMGUI_CURSOR_MOVE_DIAGONAL_ALT;
		}
		else if (border & OMGUI_LEFT_FLAG) {
			cursor = OMGUI_CURSOR_MOVE_DIAGONAL;
		}
		else {
			cursor = OMGUI_CURSOR_MOVE_VERTICAL;
		}
	}
	else if (border & OMGUI_DOWN_FLAG) {
		if (border & OMGUI_RIGHT_FLAG) {
			cursor = OMGUI_CURSOR_MOVE_DIAGONAL;
		}
		else if (border & OMGUI_LEFT_FLAG) {
			cursor = OMGUI_CURSOR_MOVE_DIAGONAL_ALT;
		}
		else {
			cursor = OMGUI_CURSOR_MOVE_VERTICAL;
		}
	}
	else if (border == OMGUI_RIGHT_FLAG || border == OMGUI_LEFT_FLAG) {
		cursor = OMGUI_CURSOR_MOVE_HORIZONTAL;
	}
	return cursor;
}

static void OmGui_HiddenTabSetActive(OmGuiContext* context, u32 tabIndex, u32 hiddenIndex, u32 chainIndex) {
	OmGui_Tab* tab = &context->tabs[tabIndex];
	OmGui_Tab* hiddenTab = &context->tabs[hiddenIndex];
	OmGui_Tab tmp = *tab;
	u32 offset = tab->button.offsetX;

	*tab = *hiddenTab;
	*hiddenTab = tmp;
	tab->rect = tmp.rect;
	tab->chainIndex = chainIndex;

	hiddenTab->isHidden = tab->isHidden;
	tab->isHidden = tmp.isHidden;

	OmGui_Swap(&tab->nextIndex, &hiddenTab->nextIndex);
	OmGui_Swap(&tab->index, &hiddenTab->index);

	context->tabIDToTabHandle[tab->id] = tabIndex;
	context->tabIDToTabHandle[hiddenTab->id] = hiddenIndex;

	{ // @HACK when hidden tab is moved and then showed, last position is rendered, reset positions
		for (u32 i = 0; i < tab->elements.count_; ++i) 
			tab->elements[i].rect = OmGui_RectCreate(0,0,0,0);
	}
}

static bool OmGui_ElementRectHot(OmGuiContext* context, const OmGui_Tab* tab, i32 elementIndex, bool setActive, const OmGui_Rect* rect) {
	if (OmGui_ElementAllowInput(context, tab) && OmGui_PointInGuiRect(context->input.mouseX, context->input.mouseY, rect)) {
		OmGui_TabElementSetHot(context, tab->index, elementIndex);
		OmGui_SetActive(context, tab->index, setActive);
		return true;
	}

	return false;
}

static OmGui_Element* OmGui_AddElement(OmGuiContext* context, OmGui_ElementType type, const char* text, bool* optOutNew) {
	OmGui_Tab* tab = context->currentTab;
	u32 hash = text ? OmGui_Hash(text) : 0;

	if (context->currentElement >= tab->elements.count_) {
		OmGui_Element newElement;
		newElement.rect = OmGui_RectCreate(-900, -900, 0, 0);
		newElement.index = tab->elements.count_;
		newElement.type = OMGUI_INVALID;
		OmGui_ArrayAdd(tab->elements, newElement, context->allocator);
	}
	// else { maybe reset rect ? }

	OmGui_Element* element = &tab->elements[context->currentElement];
	element->text = text;
	element->type = type;
	element->hash = hash;

	if (optOutNew)
		*optOutNew = element->type != type || element->hash != hash;

	context->currentElement++;
	return element;
}

static void OmGui_TabSetActive(OmGuiContext* context, u32 tabIndex) {
	if (tabIndex == context->tabs.count_ - 1)
		return;

	VerifyChains(context);

	OmGui_Tab newActive = context->tabs[tabIndex];
	OmGui_ArrayRemoveOrdered(context->tabs, tabIndex);
	OmGui_ArrayAdd(context->tabs, newActive, context->allocator);

	for (u32 i = 0; i < context->tabs.count_; ++i) {
		OmGui_Tab* tab = &context->tabs[i];
		context->tabIDToTabHandle[tab->id] = i;
		tab->index = i;

		if (tab->nextIndex == tabIndex) { // was pointing to swapped
			tab->nextIndex = context->tabs.count_ - 1;
		}
		else if (tab->nextIndex < INVALID_ARRAY_INDEX && tab->nextIndex > tabIndex) {
			tab->nextIndex--; // was moved to the left
		}
	}

	VerifyChains(context);
}

static void OmGui_TabDock(OmGuiContext* context, OmGui_Tab* dockTab, u32 tabIndex, u32 chainIndex) {
	VerifyChains(context);

	OmGui_Tab* hiddenTab = &context->tabs[tabIndex];
	hiddenTab->isHidden = true;

	if (dockTab->nextIndex == INVALID_ARRAY_INDEX)
		chainIndex = 0;

	if (chainIndex > 0) {
		OmGui_Tab* nextHiddenTab = &context->tabs[dockTab->nextIndex];
		u32 i = 1;
		while (i < chainIndex && nextHiddenTab->nextIndex != INVALID_ARRAY_INDEX) {
			nextHiddenTab = &context->tabs[nextHiddenTab->nextIndex];
			i++;
		}

		hiddenTab->nextIndex = nextHiddenTab->nextIndex;
		nextHiddenTab->nextIndex = tabIndex;
	}

	if (chainIndex == 0) {
		hiddenTab->chainIndex = INVALID_ARRAY_INDEX; // debug test

		if (dockTab->nextIndex != INVALID_ARRAY_INDEX) {
			OmGui_Tab* lastHiddenTab = hiddenTab;
			while (lastHiddenTab->nextIndex != INVALID_ARRAY_INDEX) {
				lastHiddenTab = &context->tabs[lastHiddenTab->nextIndex];
			}

			lastHiddenTab->nextIndex = dockTab->nextIndex;
		}
		dockTab->nextIndex = hiddenTab->index;
	}

	VerifyChains(context);
}

static void OmGui_HiddenTabUndock(OmGuiContext* context, OmGui_Rect rect, u32 hiddenIndex) {
	Assert(hiddenIndex != INVALID_ARRAY_INDEX);

	OmGui_Tab* hiddenTab = &context->tabs[hiddenIndex];
	for (u32 i = 0; i < context->tabs.count_; ++i) {
		if (context->tabs[i].nextIndex == hiddenIndex) {
			context->tabs[i].nextIndex = hiddenTab->nextIndex;
			break;
		}
	}

	hiddenTab->nextIndex = INVALID_ARRAY_INDEX;
	hiddenTab->isHidden = false;
	hiddenTab->chainIndex = 0;
}


static void OmGui_BufferReserve(OmGuiIAllocator* allocator, char** inOutBuffer, u32 bufferSize, u32* inOutBufferCapacity, u32 reserveSize) {
	u32 capacity = *inOutBufferCapacity;

	u32 left = capacity - bufferSize;
	if (left < reserveSize) {
		char* buffer = *inOutBuffer;

		u32 newCapacity = 2 * capacity;
		if (newCapacity - bufferSize < reserveSize) {
			newCapacity += 2 * reserveSize;
		}

		char* newData = (char*) allocator->Allocate(allocator, newCapacity);

		if (buffer) {
			memcpy(newData, buffer, bufferSize);
			allocator->Deallocate(allocator, buffer);
		}

		*inOutBuffer = newData;
		*inOutBufferCapacity = newCapacity;
	}
}


static u32 OmGui_PushBufferData(OmGuiContext* context, const char* data, u32 size) {
	Assert(context->renderDataSize == 0); // render data cannot be in use if this is !!!

	OmGui_BufferReserve(context->allocator, &context->bufferData, context->bufferDataSize, &context->bufferDataCapacity, size);
	Assert(context->bufferDataCapacity - context->bufferDataSize >= size);

	u32 result = context->bufferDataSize;
	if (data) {
		memcpy(context->bufferData + context->bufferDataSize, data, size);
	}

	context->bufferDataSize += size;
	return result;
}

static char* OmGui_ContextPushRenderData(OmGuiContext* context, const char* data, u32 size) {
	OmGui_BufferReserve(context->allocator, &context->renderData, context->renderDataSize, &context->renderDataCapacity, size);
	Assert(context->renderDataCapacity - context->renderDataSize >= size);

	char* dest = context->renderData + context->renderDataSize;
	memcpy(dest, data, size);

	context->renderDataSize += size;
	return dest;
}

static void OmGui_RectCommand(OmGuiContext* context, i32 x, i32 y, i32 w, i32 h, i32 color) {
	OmGuiRectCommandData command;
	command.type = OMGUI_COMMAND_RECT;
	command.x = x;
	command.y = y;
	command.w = w;
	command.h = h;
	command.color = color;
	OmGui_ContextPushRenderData(context, (const char*) &command, sizeof(command));
}

static void OmGui_TriangleCommand(OmGuiContext* context, i32 x, i32 y, i32 size, i32 rotDeg, i32 color) {
	OmGuiTriangleCommandData command;
	command.type = OMGUI_COMMAND_TRIANGLE;
	command.x = x;
	command.y = y;
	command.size = size;
	command.rotDeg = rotDeg;
	command.color = color;
	OmGui_ContextPushRenderData(context, (const char*) &command, sizeof(command));
}

static void OmGui_CStringCommand(OmGuiContext* context, const char* text, i32 x, i32 y, i32 color) {
	OmGuiCStringCommandData command;
	command.type = OMGUI_COMMAND_CSTRING;
	command.x = x;
	command.y = y;
	command.text = text;
	command.color = color;
	OmGui_ContextPushRenderData(context, (const char*) &command, sizeof(command));
}

static void OmGui_ScissorOnCommand(OmGuiContext* context, i32 x, i32 y, i32 w, i32 h) {
	OmGuiScissorOnCommandData command;
	command.type = OMGUI_COMMAND_SCISSOR_ON;
	command.x = x;
	command.y = y;
	command.w = w;
	command.h = h;
	OmGui_ContextPushRenderData(context, (const char*) &command, sizeof(command));
}

static void OmGui_ScissorOffCommand(OmGuiContext* context) {
	OmGuiCommandType type = OMGUI_COMMAND_SCISSOR_OFF;
	OmGui_ContextPushRenderData(context, (const char*) &type, sizeof(type));
}

static void OmGui_UserCanvasCommand(OmGuiContext* context, i32 x, i32 y, i32 w, i32 h, u8 id) {
	OmGuiUserCanvasCommandData command;
	command.type = OMGUI_COMMAND_USER_CANVAS;
	command.id = id;
	command.x = x; 
	command.y = y; 
	command.w = w; 
	command.h = h; 
	OmGui_ContextPushRenderData(context, (const char*) &command, sizeof(command));
}