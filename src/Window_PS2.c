#include "Core.h"
#if defined CC_BUILD_PS2
#include "Window.h"
#include "Platform.h"
#include "Input.h"
#include "Event.h"
#include "Graphics.h"
#include "String.h"
#include "Funcs.h"
#include "Bitmap.h"
#include "Errors.h"
#include "ExtMath.h"
#include "Logger.h"
#include <loadfile.h>
#include <libpad.h>
#include <gsKit.h>

static cc_bool launcherMode;
static char padBuf[256] __attribute__((aligned(64)));
static GSGLOBAL *gsGlobal = NULL;

struct _DisplayData DisplayInfo;
struct _WinData WindowInfo;
// no DPI scaling on PS Vita
int Display_ScaleX(int x) { return x; }
int Display_ScaleY(int y) { return y; }

static void LoadModules(void) {
    int ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    if (ret < 0) Platform_Log1("sifLoadModule SIO failed: %i", &ret);

    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
    if (ret < 0) Platform_Log1("sifLoadModule PAD failed: %i", &ret);
}

void Window_Init(void) {
	gsGlobal = gsKit_init_global();
      
	DisplayInfo.Width  = gsGlobal->Width;
	DisplayInfo.Height = gsGlobal->Height;
	DisplayInfo.Depth  = 4; // 32 bit
	DisplayInfo.ScaleX = 1;
	DisplayInfo.ScaleY = 1;
	
	WindowInfo.Width   = gsGlobal->Width;
	WindowInfo.Height  = gsGlobal->Height;
	WindowInfo.Focused = true;
	WindowInfo.Exists  = true;

	Input.Sources = INPUT_SOURCE_GAMEPAD;
	DisplayInfo.ContentOffset = 10;
	
	Platform_Log2("SIZE: %i x %i", &WindowInfo.Width, &WindowInfo.Height);
	
	LoadModules();
	padInit(0);
	padPortOpen(0, 0, padBuf);
}

void Window_Create2D(int width, int height) { 
	launcherMode = true;
	Gfx_Create(); // launcher also uses RSX to draw
}

void Window_Create3D(int width, int height) { 
	launcherMode = false; 
}

void Window_SetTitle(const cc_string* title) { }
void Clipboard_GetText(cc_string* value) { }
void Clipboard_SetText(const cc_string* value) { }

int Window_GetWindowState(void) { return WINDOW_STATE_FULLSCREEN; }
cc_result Window_EnterFullscreen(void) { return 0; }
cc_result Window_ExitFullscreen(void)  { return 0; }
int Window_IsObscured(void)            { return 0; }

void Window_Show(void) { }
void Window_SetSize(int width, int height) { }

void Window_Close(void) {
	Event_RaiseVoid(&WindowEvents.Closing);
}


/*########################################################################################################################*
*----------------------------------------------------Input processing-----------------------------------------------------*
*#########################################################################################################################*/
static void HandleButtons(int buttons) {
	// Confusingly, it seems that when a bit is on, it means the button is NOT pressed
	// So just flip the bits to make more sense
	buttons = buttons ^ 0xFFFF;
	//Platform_Log1("BUTTONS: %h", &buttons);
	
	Input_SetNonRepeatable(CCPAD_A, buttons & PAD_TRIANGLE);
	Input_SetNonRepeatable(CCPAD_B, buttons & PAD_SQUARE);
	Input_SetNonRepeatable(CCPAD_X, buttons & PAD_CROSS);
	Input_SetNonRepeatable(CCPAD_Y, buttons & PAD_CIRCLE);
      
	Input_SetNonRepeatable(CCPAD_START,  buttons & PAD_START);
	Input_SetNonRepeatable(CCPAD_SELECT, buttons & PAD_SELECT);

	Input_SetNonRepeatable(CCPAD_LEFT,   buttons & PAD_LEFT);
	Input_SetNonRepeatable(CCPAD_RIGHT,  buttons & PAD_RIGHT);
	Input_SetNonRepeatable(CCPAD_UP,     buttons & PAD_UP);
	Input_SetNonRepeatable(CCPAD_DOWN,   buttons & PAD_DOWN);
	
	Input_SetNonRepeatable(CCPAD_L,  buttons & PAD_L1);
	Input_SetNonRepeatable(CCPAD_R,  buttons & PAD_R1);
	Input_SetNonRepeatable(CCPAD_ZL, buttons & PAD_L2);
	Input_SetNonRepeatable(CCPAD_ZR, buttons & PAD_R2);
}

static void HandleJoystick_Left(int x, int y) {
	if (Math_AbsI(x) <= 8) x = 0;
	if (Math_AbsI(y) <= 8) y = 0;	
	
	if (x == 0 && y == 0) return;
	Input.JoystickMovement = true;
	Input.JoystickAngle    = Math_Atan2(x, -y);
}
static void HandleJoystick_Right(int x, int y, double delta) {
	float scale = (delta * 60.0) / 16.0f;
	
	if (Math_AbsI(x) <= 8) x = 0;
	if (Math_AbsI(y) <= 8) y = 0;
	
	Event_RaiseRawMove(&PointerEvents.RawMoved, x * scale, y * scale);	
}

static void ProcessPadInput(double delta, struct padButtonStatus* pad) {
	HandleButtons(pad->btns);
	HandleJoystick_Left( pad->ljoy_h - 0x80, pad->ljoy_v - 0x80);
	HandleJoystick_Right(pad->rjoy_h - 0x80, pad->rjoy_v - 0x80, delta);
}

void Window_ProcessEvents(double delta) {
    struct padButtonStatus pad;
	Input.JoystickMovement = false;
	
	int state = padGetState(0, 0);
    if (state != PAD_STATE_STABLE) return;
    
	int ret = padRead(0, 0, &pad);
	if (ret == 0) return;
	ProcessPadInput(delta, &pad);
}

void Cursor_SetPosition(int x, int y) { } // Makes no sense for PS Vita

void Window_EnableRawMouse(void)  { Input.RawMode = true; }
void Window_UpdateRawMouse(void)  {  }
void Window_DisableRawMouse(void) { Input.RawMode = false; }


/*########################################################################################################################*
*------------------------------------------------------Framebuffer--------------------------------------------------------*
*#########################################################################################################################*/
static struct Bitmap fb_bmp;
void Window_AllocFramebuffer(struct Bitmap* bmp) {
	bmp->scan0 = (BitmapCol*)Mem_Alloc(bmp->width * bmp->height, 4, "window pixels");
	fb_bmp     = *bmp;
}

void Window_DrawFramebuffer(Rect2D r) {
}

void Window_FreeFramebuffer(struct Bitmap* bmp) {
	Mem_Free(bmp->scan0);
}


/*########################################################################################################################*
*------------------------------------------------------Soft keyboard------------------------------------------------------*
*#########################################################################################################################*/
void Window_OpenKeyboard(struct OpenKeyboardArgs* args) { /* TODO implement */ }
void Window_SetKeyboardText(const cc_string* text) { }
void Window_CloseKeyboard(void) { /* TODO implement */ }


/*########################################################################################################################*
*-------------------------------------------------------Misc/Other--------------------------------------------------------*
*#########################################################################################################################*/
void Window_ShowDialog(const char* title, const char* msg) {
	/* TODO implement */
	Platform_LogConst(title);
	Platform_LogConst(msg);
}

cc_result Window_OpenFileDialog(const struct OpenFileDialogArgs* args) {
	return ERR_NOT_SUPPORTED;
}

cc_result Window_SaveFileDialog(const struct SaveFileDialogArgs* args) {
	return ERR_NOT_SUPPORTED;
}
#endif