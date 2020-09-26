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

#define error_messagew(...) error_messagew(sizeof((wchar_t const*[]){__VA_ARGS__}) / sizeof(wchar_t const *), (wchar_t const*[]){__VA_ARGS__})
#define WriteFile(filepath, ...) if(!WriteFile(__VA_ARGS__)) { error_messagew(L"Error could not write to ", filepath); }

typedef enum comment_display
{
    NO_COMMENT_DISPLAY = 0x0,
    C_COMMENT_DISPLAY = 0x1,
    CC_COMMENT_DISPLAY = 0x2,
    ASM_COMMENT_DISPLAY = 0x4,
    DEFAULT_COMMENT_DISPLAY = C_COMMENT_DISPLAY | CC_COMMENT_DISPLAY,
    ALL_COMMENT_DISPLAY = C_COMMENT_DISPLAY | CC_COMMENT_DISPLAY | ASM_COMMENT_DISPLAY
} comment_display;

typedef struct comment_count
{
    /* c comment count */
    size_t c_comment_count;

    /* c++ comment count */
    size_t cc_comment_count;

    /* asm comment count */
    size_t asm_comment_count;
} comment_count;

static void output_number(size_t number)
{
    /* log10(2^64) is around 20 meaning this should be able to hold all numbers inputed */
    char digits[20] = {'0'};

    /* get the reversed digets of the number */
    size_t i = number == 0 ? 1 : 0; /* check if number is zero */
    for (; number != 0; ++i) {
        digits[i] = (number % 10) + '0';
        number /= 10;
    }
    
    /* reverse the reversed digets of the number */
    for (size_t j = 0; j < i - 1; ++j) {
        char temp_digit = digits[j];
        digits[j] = digits[i - 1 - j];
        digits[i - 1 - j] = temp_digit;
    }
    
    /* print the reversed number */
    WriteFile(L"stdout", stdout, digits, i, NULL, NULL);
}

static char const *is_continuing_backslash(char const *str)
{
    /* NOTE: the loop is needed because cotinuing backslashs can nested like \\\\\ */
    if (*str != '\\') return NULL;
    while (*str != '\0') {
        switch (*str) {
            case '\r':
                break;
            case '\n':
                return str + 1;
            case '\\':
                break;
            default:
                return NULL;
        }

        ++str;
    }

    return NULL;
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
                        if (comment_mode & CC_COMMENT_DISPLAY) {
                            ++result.cc_comment_count;
                            /* add space before comment*/
                            do {
                                WriteFile(L"stdout", stdout, " ", 1, NULL, NULL);
                            } while (bytes_since_newline-- != 0);
                            ++bytes_since_newline;

                            ++str;
                            while (*str != '\0' && (*str != '\n' || (str[0] != '\r' && str[1] != '\n'))) {
                                /* if we detect a continuing backslash treat the next line as a comment */
                                char const *continuing_backslash_pos;
                                if ((continuing_backslash_pos = is_continuing_backslash(str)) != NULL) {
                                    str = continuing_backslash_pos;
                                    WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);

                                }
                                WriteFile(L"stdout", stdout, str, 1, NULL, NULL);

                                ++str;
                            }
                            WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);

                        }
                        break;

                    case '*':
                        if (comment_mode & C_COMMENT_DISPLAY) {

                            ++result.c_comment_count;
                            /* add space before comment*/
                            do {
                                WriteFile(L"stdout", stdout, " ", 1, NULL, NULL);
                            } while (bytes_since_newline-- != 0);
                            ++bytes_since_newline;

                            ++str;
                            while (*str != '\0') {
                                if (str[0] == '*' && str[1] == '/') {
                                    str += 2;
                                    break;
                                }
                                WriteFile(L"stdout", stdout, str, 1, NULL, NULL);

                                ++str;
                            }
                            WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);

                        }
                        break;
                }
                break;

            case '\n':
                bytes_since_newline = 0;
                break;

            case ';':
                if (comment_mode & ASM_COMMENT_DISPLAY) {
                    ++result.asm_comment_count;
                    /* add space before comment*/
                    do {
                        WriteFile(L"stdout", stdout, " ", 1, NULL, NULL);
                    } while (bytes_since_newline-- != 0);
                    ++bytes_since_newline;

                    ++str;
                    while (*str != '\0' && (*str != '\n' || (str[0] != '\r' && str[1] != '\n'))) {
                        WriteFile(L"stdout", stdout, str, 1, NULL, NULL);
                        ++str;
                    }

                    WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);
                }
                break;
        }
        ++str;
        bytes_since_newline += *str == '\t' ? 4 : 1; /* handle tabs */
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
                        if (comment_mode & CC_COMMENT_DISPLAY) {
                            ++result.cc_comment_count;
                            /* add space before comment*/
                            do {
                                WriteFile(L"stdout", stdout, " ", 1, NULL, NULL);
                            } while (bytes_since_newline-- != 0);
                            ++bytes_since_newline;
                            ++str;
                            while (*str != '\0') {

                                /* stop when we reach the end of the line */
                                if (str[0] == '\n' || (str[0] == '\r' && str[1] == '\n')) {
                                    WriteFile(L"stdout", stdout, " ", 1, NULL, NULL);
                                    output_number(newline_count);
                                    WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);

                                    /* NOTE: when we increment the string before going to the end of loop 
                                     * we must add to the string pointer by 1 less since we already
                                     * increment the string pointer at the end of the loop
                                     */
                                    str += str[0] == '\n' ? 0 : 1;
                                    ++newline_count;
                                    break;
                                }

                                /* if we detect \\ treat the next line as a comment */
                                char const *continuing_backslash_pos;
                                if ((continuing_backslash_pos = is_continuing_backslash(str)) != NULL) {
                                    
                                    /* since we are moving to a newline output the current line number */
                                    WriteFile(L"stdout", stdout, " ", 1, NULL, NULL);
                                    output_number(newline_count);
                                    WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);
                                    
                                    str = continuing_backslash_pos;
                                    ++newline_count;
                                }

                                WriteFile(L"stdout", stdout, str, 1, NULL, NULL);

                                ++str;
                            }
                            WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);
                        }
                        break;

                    case '*':
                        if (comment_mode & C_COMMENT_DISPLAY) {
                            ++result.c_comment_count;
                            /* add space before comment */
                            do {
                                WriteFile(L"stdout", stdout, " ", 1, NULL, NULL);
                            } while (bytes_since_newline-- != 0);
                            ++bytes_since_newline;

                            ++str;
                            while (*str != '\0') {
                                if (str[0] == '*' && str[1] == '/') {
                                    WriteFile(L"stdout", stdout, " ", 1, NULL, NULL);
                                    output_number(newline_count);

                                    ++str;
                                    if (str[1] == '\n' || (str[1] == '\r' && str[2] == '\n')) {
                                        str += str[0] == '\n' ? 1 : 2;
                                        ++newline_count;
                                    }
                                    break;
                                }

                                WriteFile(L"stdout", stdout, str, 1, NULL, NULL);

                                ++str;
                                while (str[0] == '\n' || (str[0] == '\r' && str[1] == '\n')) {

                                    /* output a number before the end of the line */
                                    WriteFile(L"stdout", stdout, " ", 1, NULL, NULL);
                                    output_number(newline_count);
                                    WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);

                                    str += str[0] == '\n' ? 1 : 2;
                                    ++newline_count;
                                }
                            }

                            WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);
                        }
                        break;
                }
                break;

            case '\n':
                bytes_since_newline = 0;
                ++newline_count;
                break;

            case ';':
                if (comment_mode & ASM_COMMENT_DISPLAY) {
                    ++result.asm_comment_count;
                    /* add space before comment*/
                    do {
                        WriteFile(L"stdout", stdout, " ", 1, NULL, NULL);
                    } while (bytes_since_newline-- != 0);
                    ++bytes_since_newline;

                    ++str;
                    while (*str != '\0' && (*str != '\n' || (str[0] != '\r' && str[1] != '\n'))) {
                        WriteFile(L"stdout", stdout, str, 1, NULL, NULL);
                        ++str;
                        if (*str == '\n' || (str[0] == '\r' && str[1] == '\n')) {
                            WriteFile(L"stdout", stdout, " ", 1, NULL, NULL);
                            output_number(newline_count);

                            ++newline_count;
                        }
                    }

                    WriteFile(L"stdout", stdout, "\r\n", 2, NULL, NULL);
                }
                break;
        }
        ++str;
        bytes_since_newline += *str == '\t' ? 4 : 1; /* handle tabs */
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

            if (comment_mode & ASM_COMMENT_DISPLAY) {
                WriteFile(L"stdout", stdout, "\r\nasm style comments: ", 22, NULL, NULL);
                output_number(count.asm_comment_count);
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

/* TODO: consider supporting python style comments #*/
void __cdecl mainCRTStartup(void)
{
    static wchar_t const *help_message = L"Usage: comments [-l or --line] [-nl or --no_line] [-e [mode] or --enable=[mode]] [-m [mode] or --mode=[mode]] [-d [mode] or --disable=[mode]] [--display_comment_count or -dcc] [--hide_comment_count -hcc] file1 ...\n\
                                      -l or --line(disabled by default): makes it so the program shows the line number of each comment\n\
                                      -nl or --no_line: has the oppsite effect of -l\n\
                                      -e [mode] or --enable=[mode]: enables the comment mode to [mode] for example -e asm enables asm comments\n\
                                      -m [mode] or --mode=[mode]: sets the comment mode  to [mode] for example -m asm only allows only asm comments\n\
                                      -d [mode] or --disable=[mode]: disables the comment mode to [mode] for example -d asm disables asm comments\n\
                                      [mode](the default mode is c and c++): the different modes are c++ style comments //(cc, cxx, cpp), c style comments /**/ (c), asm style comments ;(asm), default /**/ //(default), and all which enables all the available comments(all) \n\
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
    comment_display comment_mode = CC_COMMENT_DISPLAY | C_COMMENT_DISPLAY;

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
            } else if (!lstrcmpiW(argv[i], L"asm")) {
                comment_mode = ASM_COMMENT_DISPLAY;
            } else if (!!lstrcmpiW(argv[i], L"default")) {
                comment_mode = DEFAULT_COMMENT_DISPLAY;
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
            } else if (!lstrcmpiW(argv[i], L"asm")) {
                comment_mode = ASM_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"default")) {
                comment_mode = DEFAULT_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"all")) {
                comment_mode = ALL_COMMENT_DISPLAY;
            } else {
                error_messagew(L"Error: invalid arguments\n", help_message);
            }
        } else if (argv[i][0] == '-' && argv[i][1] == '-'
            && argv[i][2] == 'e' && argv[i][3] == 'n'
            && argv[i][4] == 'a' && argv[i][5] == 'b'
            && argv[i][6] == 'l' && argv[i][7] == 'e'
            && argv[i][8] == '=') {
            argv[i] += 9;
            if (!lstrcmpiW(argv[i], L"cc") || !lstrcmpiW(argv[i], L"cxx")
                || !lstrcmpiW(argv[i], L"cpp")) {
                comment_mode |= CC_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"c")) {
                comment_mode |= C_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"asm")) {
                comment_mode |= ASM_COMMENT_DISPLAY;
            } else if(!lstrcmpiW(argv[i], L"default")) {
                comment_mode |= DEFAULT_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"all")) {
                comment_mode |= ALL_COMMENT_DISPLAY;
            } else {
                error_messagew(L"Error: invalid arguments\n", help_message);
            }
        } else if (i + 1 < argc && !lstrcmpW(argv[i], L"-e")) {
            ++i;
            if (!lstrcmpiW(argv[i], L"cc") || !lstrcmpiW(argv[i], L"cxx")
                || !lstrcmpiW(argv[i], L"cpp")) {
                comment_mode |= CC_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"c")) {
                comment_mode |= C_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"asm")) {
                comment_mode |= ASM_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"default")) {
                comment_mode |= DEFAULT_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"all")) {
                comment_mode |= ALL_COMMENT_DISPLAY;
            } else {
                error_messagew(L"Error: invalid arguments\n", help_message);
            }
        } else if (argv[i][0] == '-' && argv[i][1] == '-'
            && argv[i][2] == 'd' && argv[i][3] == 'i'
            && argv[i][4] == 's' && argv[i][5] == 'a'
            && argv[i][6] == 'b' && argv[i][7] == 'l'
            && argv[i][8] == 'e' && argv[i][9] == '=') {
            argv[i] += 10;
            if (!lstrcmpiW(argv[i], L"cc") || !lstrcmpiW(argv[i], L"cxx")
                || !lstrcmpiW(argv[i], L"cpp")) {
                comment_mode &= ~CC_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"c")) {
                comment_mode &= ~C_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"asm")) {
                comment_mode &= ~ASM_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"default")) {
                comment_mode &= ~DEFAULT_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"all")) {
                comment_mode &= ~ALL_COMMENT_DISPLAY;
            } else {
                error_messagew(L"Error: invalid arguments\n", help_message);
            }
        } else if (i + 1 < argc && !lstrcmpW(argv[i], L"-d")) {
            ++i;
            if (!lstrcmpiW(argv[i], L"cc") || !lstrcmpiW(argv[i], L"cxx")
                || !lstrcmpiW(argv[i], L"cpp")) {
                comment_mode &= ~CC_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"c")) {
                comment_mode &= ~C_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"asm")) {
                comment_mode &= ~ASM_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"default")) {
                comment_mode &= ~DEFAULT_COMMENT_DISPLAY;
            } else if (!lstrcmpiW(argv[i], L"all")) {
                comment_mode &= ~ALL_COMMENT_DISPLAY;
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