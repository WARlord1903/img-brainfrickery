#include "brainfrick.h"

struct bf_t bf = {NULL, 0};
char* frame = NULL;
size_t frame_size;

#ifdef _WIN32
HANDLE* hconsole;

BOOL WINAPI consoleHandler(DWORD signal){
    if(signal == CTRL_C_EVENT)
        exit(1);
    return false;
}

void cls(void){
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    CHAR_INFO fill;

    GetConsoleScreenBufferInfo(hconsole, &csbi);

    SMALL_RECT scrollRect = {0, 0, csbi.dwSize.X, csbi.dwSize.Y};
    COORD scrollTarget = {0, (SHORT)(0 - csbi.dwSize.Y)};

    fill.Char.UnicodeChar = TEXT(' ');
    fill.Attributes = csbi.wAttributes;

    ScrollConsoleScreenBuffer(hconsole, &scrollRect, NULL, scrollTarget, &fill);

    csbi.dwCursorPosition.X = 0;
    csbi.dwCursorPosition.Y = 0;

    SetConsoleCursorPosition(hconsole, csbi.dwCursorPosition);
}

void print_callback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR p1, DWORD_PTR p2){
    const COORD start_pos = {0, 0};
    struct timer_data_t* ptr = (struct timer_data_t*) dwUser;
    WriteConsole(hconsole, ptr->frame, ptr->frame_size, NULL, NULL);
    SetConsoleCursorPosition(hconsole, start_pos);
}
#endif

size_t parse_loop(const char* code, size_t start, bool buffering, double framerate){
    size_t remaining_closing_brackets = 0;
    size_t i;
    for(i = start; i < strlen(code); i++){
        if(code[i] == '[')
            remaining_closing_brackets++;
        else if(code[i] == ']')
            remaining_closing_brackets--;
        if(remaining_closing_brackets == 0)
            break;
    }
    if(remaining_closing_brackets > 0)
        exit(EXIT_FAILURE);
    char* loop_contents = (char*) malloc(i - start + 1);
    strncpy(loop_contents, &code[start + 1], i - start);
    loop_contents[i - start] = '\0';

    while(bf.buf[bf.pos])
        interpret_code(loop_contents, buffering, framerate);
    
    free(loop_contents);
    return i;
}

void interpret_code(const char* code, bool buffering, double framerate){
    static size_t char_count = 0;
    #ifdef _WIN32
        static LARGE_INTEGER epoch = {0};
        if(epoch.QuadPart == 0){
            QueryPerformanceCounter(&epoch);
            SetConsoleCtrlHandler(consoleHandler, true);
        }
        const double frame_time = 1. / framerate * 1000000000;
        static size_t curr_frame;
    #else
        static struct timespec start = {0};
        struct timespec end;
        if(start.tv_sec == 0)
            clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    #endif

    for(size_t i = 0; i < strlen(code); i++){
        switch(code[i]){
            case '<':
                if(bf.pos > 0)
                    --bf.pos;
                else
                    bf.pos = BF_BUFFER_SIZE - 1;
                break;
            case '>':
                if(bf.pos < BF_BUFFER_SIZE - 1)
                    ++bf.pos;
                else
                    bf.pos = 0;
                break;
            case '+':
                bf.buf[bf.pos]++;
                break;
            case '-':
                bf.buf[bf.pos]--;
                break;
            case '.':
                if(buffering){
                    frame[char_count++] = (char) bf.buf[bf.pos];
                    if(char_count == frame_size){
                        #ifndef _WIN32
                            mvprintw(0, 0, "%s", frame);
                            refresh();
                            clock_gettime(CLOCK_MONOTONIC_RAW, &end);
                            struct timespec req;
                            req.tv_sec = 0;
                            req.tv_nsec = (1. / framerate * 1000000000) - (end.tv_nsec - start.tv_nsec);
                            nanosleep(&req, NULL);
                            clock_gettime(CLOCK_MONOTONIC_RAW, &start);
                        #else
                            const COORD start_pos = {0, 0};
                            LARGE_INTEGER end;
                            LARGE_INTEGER frequency;
                            QueryPerformanceFrequency(&frequency);
                            SetConsoleCursorPosition(hconsole, start_pos);
                            WriteConsole(hconsole, frame, frame_size, NULL, NULL);
                            QueryPerformanceCounter(&end);
                            while(epoch.QuadPart * 1000000000. / frequency.QuadPart + frame_time * curr_frame > end.QuadPart * 1000000000. / frequency.QuadPart)
                                QueryPerformanceCounter(&end);
                            curr_frame++;
                        #endif

                        char_count = 0;
                    }
                }
                else
                    fputc((char) bf.buf[bf.pos], stdout);
                break;
            case ',':
                bf.buf[bf.pos] = (uint8_t) fgetc(stdin);
                break;
            case '[':
                size_t end_pos = parse_loop(code, i, buffering, framerate);
                i = end_pos;
                break;
        }
    }
}

void init_bf(void){
    if(bf.buf)
        free(bf.buf);
    bf.buf = (uint8_t*) calloc(BF_BUFFER_SIZE, sizeof(uint8_t));
    bf.pos = 0;
}

void end_bf(void){
    free(bf.buf);
    if(frame)
        free(frame);
    #ifndef _WIN32
        fflush(stdout);
        endwin();
    #else
        cls();
    #endif
}

void set_frame_size(size_t s){
    if(frame)
        free(frame);
    frame = malloc(s + 1);
    frame_size = s;
}

void init_display(void){
    #ifndef _WIN32
        initscr();
    #else
        hconsole = GetStdHandle(STD_OUTPUT_HANDLE);
        cls();
    #endif
}