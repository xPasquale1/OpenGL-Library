#include "windowgl.h"
#include "font.h"

std::vector<CircleData> circles;
std::vector<RectangleData> rectangles;
std::vector<LineData> lines;
std::vector<CharData> chars;

INT WinMain(HINSTANCE hInstance, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow){
 Window window;
    if(ErrCheck(createWindow(window, hInstance, 1000, 1000, 0, 0, 1, "Fenster"), "Fenster öffnen") != SUCCESS) return -1;
    if(init() != SUCCESS) return -1;

    Font font;
    if(ErrCheck(loadTTF(font, "fonts/OpenSans-Bold.ttf"), "Font laden") != SUCCESS) return -1;

    wglSwapIntervalEXT(0);

    Timer timer;
    DWORD timeTaken = 0;
    std::string fpsText = "FPS: 0";

    while(1){
        resetTimer(timer);
        getMessages(window);
        if(getWindowFlag(window, WINDOW_CLOSE)) break;
        clearWindow(window);

        renderCircles(window, circles.data(), circles.size());
        renderRectangles(window, rectangles.data(), rectangles.size());
        drawFontString(window, font, chars, fpsText.c_str(), 10, 10);
        renderLines(window, lines.data(), lines.size());

        renderFontChars(window, font, chars.data(), chars.size());
        drawWindow(window);
        timeTaken = getTimerMicros(timer);
        fpsText = "FPS: " + std::string(longToString(1000000/timeTaken));
        rectangles.clear();
        circles.clear();
        lines.clear();
        chars.clear();

        if(getButton(mouse, MOUSE_LMB)) setButton(mouse, MOUSE_PREV_LMB);
        else resetButton(mouse, MOUSE_PREV_LMB);
    }

    destroyFont(font);
    if(ErrCheck(destroyWindow(window), "Fenster schließen") != SUCCESS) return -1;
    return 0;
}
