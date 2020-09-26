#include <windows.h>
#include <stdarg.h>
#include <stdbool.h>

HANDLE stdout = NULL;
HANDLE stderr = NULL;

__declspec(noreturn) static void error_messagew(size_t count, wchar_t const **messages)
{
    for (size_t i = 0; i < count; ++i) {
        wchar_t const *message = messages[i];
        WriteConsoleW(stderr, message, lstrlenW(message), NULL, NULL);
    }
    ExitProcess(GetLastError());
}

#define error_messagew(...) error_messagew(sizeof((wchar_t const*[]){__VA_ARGS__}) / sizeof(wchar_t const *), (wchar_t const*[]){__VA_ARGS__});
#define WriteFile(filepath, ...) if(!WriteFile(__VA_ARGS__)) { error_messagew(L"Error could not write to ", filepath); }

typedef enum comment_display
{
    NO_COMMENT_DISPLAY = 0x0,
    C_COMMENT_DISPLAY = 0x1,
    CC_COMMENT_DISPLAY = 0x2,
    ALL_COMMENT_DISPLAY = C_COMMENT_DISPLAY | CC_COMMENT_DISPLAY
} comment_display;

typedef struct comment_count
{
    /* c comment count */
    size_t c_comment_count;

    /* c++ comment count */
    size_t cc_comment_count;
} comment_count;

static void output_number(size_t number)
{
    size_t reversed_number = 0;

    /* get the reversed version of the number  */
    while (number != 0) {
        reversed_number *= 10;
        reversed_number += number % 10;
        number /= 10;
    }
    
    /* print the reversed number */
    do {
        WriteFile(L"stdout", stdout, (char[]) { (reversed_number % 10) + '0' }, 1, NULL, NULL);
        reversed_number /= 10;
    } while (reversed_number != 0);
}

/* NOTE: this function requires a null terminated string */
static comment_count read_comments(char const *str, comment_display comment_mode)
{
    comment_count result = { 0 };

    /* keep reading the next char until we reach a null terminator*/
    size_t bytes_since_newline = 0;
    while (*str != '\0') {
        switch (*str) {
            /* handle "" and '' */
            case '"':
            case '\'': {
                /* we need to read the current char to know what type of quote we are using
                 * once we have already read the current char so we need to go to the next one
                 */
                char quote_type = *str++;

                while (*str != '0' && *str != quote_type) {
                    /* just skip escape codes as the could containe " or ' */
                    if (*str == '\\') {
                        ++str;
                    }
                    ++str;
                }
                break;
            }
           
            case '/':
                ++str;
                switch (*str) {
                    case '/':
                        ++result.cc_comment_count;
                        if (comment_mode & CC_COMMENT_DISPLAY) {
                            /* add space before comment*/
                            while (bytes_since_newline-- > 0) {
                                WriteFile(L"stdout", stdout, (char[]) { ' ' }, 1, NULL, NULL);
                            }
                        }
                        ++str;
                        while (*str != '\0' && *str != '\n') {
                            /* if we detect \\ treat the next line as a comment */
                            if (str[0] == '\\' && str[1] == '\\' && (str[2] == '\n' || (str[2] == '\r' && str[3] == '\n'))) {
                                str += 3;
                                if (comment_mode & CC_COMMENT_DISPLAY) {
                                    WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);
                                }
                            }
                            if (comment_mode & CC_COMMENT_DISPLAY) {
                                WriteFile(L"stdout", stdout, str, 1, NULL, NULL);
                            }
                            ++str;
                        }
                        if (comment_mode & CC_COMMENT_DISPLAY) {
                            WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);
                        }
                        break;

                    case '*':
                        ++result.c_comment_count;
                        if (comment_mode & C_COMMENT_DISPLAY) {
                            /* add space before comment*/
                            while (bytes_since_newline-- > 0) {
                                WriteFile(L"stdout", stdout, (char[]) { ' ' }, 1, NULL, NULL);
                            }
                        }
                        ++str;
                        while (*str != '\0') {
                            if (str[0] == '*' && str[1] == '/') {
                                str += 2;
                                break;
                            }
                            if (comment_mode & C_COMMENT_DISPLAY) {
                                WriteFile(L"stdout", stdout, str, 1, NULL, NULL);
                            }                            
                            ++str;
                        }
                        if (comment_mode & C_COMMENT_DISPLAY) {
                            WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);
                        }
                        break;
                }
                break;

            case '\n':
                bytes_since_newline = 0;
        }
        ++str;
        ++bytes_since_newline;
    }

    return result;
}

/* NOTE: this function requires a null terminated string */
static comment_count read_comments_and_line(char const *str, comment_display comment_mode)
{
    comment_count result = { 0 };

    /* keep reading the next char until we reach a null terminator*/
    size_t bytes_since_newline = 0;
    size_t newline_count = 1;
    while (*str != '\0') {
        switch (*str) {
            /* handle "" and '' */
            case '"':
            case '\'': {
                /* we need to read the current char to know what type of quote we are using
                 * once we have already read the current char so we need to go to the next one
                 */
                char quote_type = *str++;

                while (*str != '0' && *str != quote_type) {
                    /* just skip escape codes as the could containe " or ' */
                    if (*str == '\\') {
                        ++str;
                    }

                    if (*str == '\n') {
                        ++newline_count;
                    }
                    ++str;
                }
                break;
            }

            case '/':
                ++str;
                switch (*str) {
                    case '/':
                        ++result.cc_comment_count;
                        if (comment_mode & CC_COMMENT_DISPLAY) {
                            /* add space before comment*/
                            while (bytes_since_newline-- > 0) {
                                WriteFile(L"stdout", stdout, (char[]) { ' ' }, 1, NULL, NULL);
                            }
                        }
                        ++str;
                        while (*str != '\0' && *str != '\n') {
                            /* if we detect \\ treat the next line as a comment */
                            if (str[0] == '\\' && str[1] == '\\' && (str[2] == '\n' || (str[2] == '\r' && str[3] == '\n'))) {
                                str += 3;
                                if (comment_mode & CC_COMMENT_DISPLAY) {
                                    WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);
                                }
                            }
                            if (*str == '\n' || (str[0] == '\r' && str[1] == '\n')) {
                                if (comment_mode & CC_COMMENT_DISPLAY) {
                                    WriteFile(L"stdout", stdout, " ", 1, NULL, NULL);
                                    output_number(newline_count);
                                }
                                ++newline_count;
                            }
                            if (comment_mode & CC_COMMENT_DISPLAY) {
                                WriteFile(L"stdout", stdout, str, 1, NULL, NULL);
                            }
                            ++str;
                            
                        }
                        if (comment_mode & CC_COMMENT_DISPLAY) {
                            WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);
                        }
                        break;

                    case '*':
                        ++result.c_comment_count;
                        if (comment_mode & C_COMMENT_DISPLAY) {
                            /* add space before comment */
                            while (bytes_since_newline-- > 0) {
                                WriteFile(L"stdout", stdout, (char[]) { ' ' }, 1, NULL, NULL);
                            }
                        }
                        ++str;
                        while (*str != '\0') {
                            if (str[0] == '*' && str[1] == '/') {
                                str += 2;
                                if (comment_mode & C_COMMENT_DISPLAY) {
                                    /* print line number */
                                    WriteFile(L"stdout", stdout, (char[]) { ' ' }, 1, NULL, NULL);
                                    output_number(newline_count);
                                }
                                break;
                            }
                            if (*str == '\n') {
                                if (comment_mode & C_COMMENT_DISPLAY) {
                                    /* print line number */
                                    WriteFile(L"stdout", stdout, (char[]) { ' ' }, 1, NULL, NULL);
                                    output_number(newline_count);
                                }
                                ++newline_count;
                            }
                            if (comment_mode & C_COMMENT_DISPLAY) {
                                WriteFile(L"stdout", stdout, str, 1, NULL, NULL);
                            }
                            ++str;
                        }
                        if (comment_mode & C_COMMENT_DISPLAY) {
                            WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);
                        }
                        break;
                }
                break;

            case '\n':
                bytes_since_newline = 0;
                ++newline_count;

        }
        ++str;
        ++bytes_since_newline;
    }

    return result;
}

static void read_file_comments(wchar_t const *filename, comment_display comment_mode, bool show_line_number, bool display_comment_count)
{
    HANDLE file_handle = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN | FILE_ATTRIBUTE_READONLY, NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        error_messagew(L"Error: could not open file \"", filename, L"\"");
    }

    /* get the file size */
    LARGE_INTEGER file_size;
    if (GetFileSizeEx(file_handle, &file_size) == FALSE) {
        error_messagew(L"Error: could not get the file size of \"", filename, L"\\");
    }

    char *file_buffer = HeapAlloc(GetProcessHeap(), 0, file_size.QuadPart + 1);
    file_buffer[file_size.QuadPart] = '\0'; /* add a null terminator */

    /* read the file into the file buffer */
    DWORD bytes_read = file_size.LowPart;
    if (ReadFile(file_handle, file_buffer, file_size.LowPart, &bytes_read, NULL) == FALSE || bytes_read != file_size.QuadPart) {
        error_messagew(L"Error: could not read ", filename);
    }

    /* process the file and read the comments */
    {
        comment_count count = show_line_number ? read_comments_and_line(file_buffer, comment_mode) : read_comments(file_buffer, comment_mode);
        if (display_comment_count) {
            if (comment_mode & CC_COMMENT_DISPLAY) {
                WriteFile(L"stdout", stdout, "c++ style comments: ", 20, NULL, NULL);
                output_number(count.cc_comment_count);
            }

            if (comment_mode & C_COMMENT_DISPLAY) {
                WriteFile(L"stdout", stdout, "\r\nc style comments: ", 20, NULL, NULL);
                output_number(count.c_comment_count);
            }
        }
    }
}

static bool is_file(wchar_t const *filepath)
{
    DWORD result = GetFileAttributesW(filepath);
    if (result == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    return (result & ~FILE_ATTRIBUTE_DIRECTORY) != 0;
}

/* TODO: consider supporting asm comments i.e ; also maybe #*/
void __cdecl mainCRTStartup(void)
{
    static wchar_t const *help_message = L"Usage: comments [-l or --line] [-nl or --no_line] [-m [mode] or --mode=[mode]] [--display_comment_count or -dcc] [--hide_comment_count -hcc] file1 ...\n\
                                      -l or --line(disabled by default): makes it so the program shows the line number of each comment\n\
                                      -nl or --no_line: has the oppsite effect of -l\n\
                                      -m [mode] or --mode=[mode]: sets the comment mode for example -m cc only shows c++ comments\n\
                                      [mode](default: the different modes are c++ style comments //(cc, cxx, cpp), c style comments /**/ (c), and all which enables all the available comments(all) \n\
                                      -dcc or --display_comment_count(enabled by defualt): displays the amount of comments found \n\
                                      -hcc or --hides_comment_count: hides the amount of comments found \n\
                                     ";
    stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    stderr = GetStdHandle(STD_ERROR_HANDLE);

    /* get command line args */
    int argc;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc) + 1;
    --argc;
    
    bool show_lines = false;
    bool display_comment_count = true;
    comment_display comment_mode = ALL_COMMENT_DISPLAY;

    /* parse command line args */
    for (int i = 0; i < argc; ++i) {
        if (!lstrcmpW(argv[i], L"--line") || !lstrcmpW(argv[i], L"-l")) {
            show_lines = true;
        } else if (!lstrcmpW(argv[i], L"--no_line") || !lstrcmpW(argv[i], L"-nl")) {
            show_lines = false;
        } else if (!lstrcmpW(argv[i], L"--display_comment_count") || !lstrcmpW(argv[i], L"-dcc")) {
            display_comment_count = true;
        } else if (!lstrcmpW(argv[i], L"--hide_comment_count") || !lstrcmpW(argv[i], L"-hcc")) {
            display_comment_count = false;
        } else if (argv[i][0] == '-' && argv[i][1] == '-'
            && argv[i][2] == 'm' && argv[i][3] == 'o'
            && argv[i][4] == 'd' && argv[i][5] == 'e'
            && argv[i][6] == '=') {
            argv[i] += 7;
            if (!lstrcmpiW(argv[i], L"cc") || !lstrcmpiW(argv[i], L"cxx")
                || !lstrcmpiW(argv[i], L"cpp")) {
                comment_mode = CC_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"c")) {
                comment_mode = C_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"all")) {
                comment_mode = ALL_COMMENT_DISPLAY;
            } else {
                error_messagew(L"Error: invalid arguments\n", help_message);
            }
        } else if (i + 1 < argc && !lstrcmpW(argv[i], L"-m")) {
            ++i;
            if (!lstrcmpiW(argv[i], L"cc") || !lstrcmpiW(argv[i], L"cxx")
                || !lstrcmpiW(argv[i], L"cpp")) {
                comment_mode = CC_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"c")) {
                comment_mode = C_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"all")) {
                comment_mode = ALL_COMMENT_DISPLAY;
            } else {
                error_messagew(L"Error: invalid arguments\n", help_message);
            }
        } else if (is_file(argv[i])) {
            read_file_comments(argv[i], comment_mode, show_lines, display_comment_count);
        } else {
            error_messagew(L"Error: invalid arguments\n", help_message);
        }
    }

    /* cleanup */
    LocalFree(argv - 1);

    /* success */
    ExitProcess(0);
}
