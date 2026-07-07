#include "App/Application.h"

#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    App::Application app;
    return app.run(instance);
}

