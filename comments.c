#include <windows.h>
#include <stdbool.h>

#include "argva.c"

HANDLE stdout = NULL;
HANDLE stderr = NULL;

__declspec(noreturn) static void error_messagea(size_t count, char const **messages)
{
    for (size_t i = 0; i < count; ++i) {
        char const *message = messages[i];
        WriteFile(stderr, message, lstrlenA(message), NULL, NULL);
    }
    ExitProcess(GetLastError());
}

#define error_messagea(...) error_messagea(sizeof((char const*[]){__VA_ARGS__}) / sizeof(char const *), (char const*[]){__VA_ARGS__})
#define WriteFile(filepath, ...) if(!WriteFile(__VA_ARGS__)) { error_messagea("Error could not write to ", filepath); }

typedef enum comment_display
{
    NO_COMMENT_DISPLAY = 0x0,
    C_COMMENT_DISPLAY = 0x1,
    CC_COMMENT_DISPLAY = C_COMMENT_DISPLAY << 1,
    ASM_COMMENT_DISPLAY = CC_COMMENT_DISPLAY << 1,
    PYTHON_COMMENT_DISPLAY = ASM_COMMENT_DISPLAY << 1,
    RUST_COMMENT_DISPLAY = PYTHON_COMMENT_DISPLAY << 1,
    AUTO_COMMENT_DISPLAY = RUST_COMMENT_DISPLAY << 1,
    C_AND_CC_COMMENT_DISPLAY = C_COMMENT_DISPLAY | CC_COMMENT_DISPLAY,
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

    /* python comment count */
    size_t python_comment_count;

    /* rust comment count */
    size_t rust_comment_count;
} comment_count;

static void output_number(size_t number)
{
    /* log10(2^64) is around 20 meaning this should be able to hold all numbers inputed */
    char digits[20] = { '0' };

    /* get the reversed digets of the number */
    int i = number == 0 ? 1 : 0; /* check if number is zero */
    for (; number != 0; ++i) {
        digits[i] = (number % 10) + '0';
        number /= 10;
    }

    /* reverse the reversed digets of the number */
    for (int j = 0; j < i - 1; ++j) {
        char temp_digit = digits[j];
        digits[j] = digits[i - 1 - j];
        digits[i - 1 - j] = temp_digit;
    }

    /* print the reversed number */
    WriteFile("stdout", stdout, digits, i, NULL, NULL);
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

static comment_display get_comment_mode(char const *str)
{
    char const *file_extension_pos = str;

    /* find location of the file extension in the string */
    {
        char const *temp_pos = NULL;
        while (*file_extension_pos != '\0') {
            if (*file_extension_pos == '.') {
                temp_pos = file_extension_pos;
            }
            ++file_extension_pos;
        }

        if (temp_pos == NULL) {
            return C_AND_CC_COMMENT_DISPLAY;
        }

        file_extension_pos = temp_pos;
    }

    /* file extensions of languages the use c/c++ style comments */
    static char const *const cc_file_extensions[] = {
        ".c",
        ".cpp",
        ".h",
        ".hpp",
        ".cc",
        ".hh",
        ".java",
        ".cs",
        ".cu",
        ".cuh",
        ".go"
        ".hxx",
        ".cxx",
        ".c++",
        ".h++",
    };

    /* check if the file extension is of a programming language that uses c/c++ style comments */
    for (size_t i = 0; i < (sizeof(cc_file_extensions) / sizeof(char const *const)); ++i) {
        if (!lstrcmpiA(file_extension_pos, cc_file_extensions[i])) {
            return C_AND_CC_COMMENT_DISPLAY;
        }
    }

    if (!lstrcmpiA(file_extension_pos, ".asm") || !lstrcmpiA(file_extension_pos, ".s")) {
        return ASM_COMMENT_DISPLAY;
    }
    else if (!lstrcmpiA(file_extension_pos, ".py")) {
        return PYTHON_COMMENT_DISPLAY;
    }
    else if (!lstrcmpiA(file_extension_pos, ".rs")) {
        return RUST_COMMENT_DISPLAY;
    }
    else {
        return NO_COMMENT_DISPLAY;
    }
}

/* NOTE: this function requires a null terminated string */
static comment_count read_comments(char const *str, bool show_lines, comment_display comment_mode)
{
    /* check if can even display comments */
    if (comment_mode == NO_COMMENT_DISPLAY) {
        return (comment_count) { 0 };
    }

    comment_count result = { 0 };

    /* keep reading the next char until we reach a null terminator*/
    size_t bytes_since_newline = 1;
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

                /* check for python doc string */
                if (str[0] == quote_type && str[1] == quote_type) {
                    ++result.python_comment_count;
                    if ((comment_mode & PYTHON_COMMENT_DISPLAY)) {
                        /* add space before comment*/
                        do {
                            WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                        } while (bytes_since_newline-- != 0);
                        ++bytes_since_newline;

                        str += 2;
                        while (*str != '\0') {
                            if (str[0] == quote_type && str[1] == quote_type && str[2] == quote_type) {
                                if (show_lines) {
                                    WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                                    output_number(newline_count);
                                }

                                str += 2;
                                if (str[1] == '\n' || (str[1] == '\r' && str[2] == '\n')) {
                                    str += str[0] == '\n' ? 1 : 2;
                                    ++newline_count;
                                }
                                break;
                            }

                            WriteFile("stdout", stdout, str, 1, NULL, NULL);

                            ++str;

                            while (str[0] == '\n' || (str[0] == '\r' && str[1] == '\n')) {
                                if (show_lines) {
                                    /* output a number before the end of the line */
                                    WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                                    output_number(newline_count);
                                }

                                WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);

                                str += str[0] == '\n' ? 1 : 2;
                                ++newline_count;
                            }
                        }

                        WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);
                    }
                    break;
                }


                while (*str != '\0' && *str != quote_type) {
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
                        ++result.rust_comment_count;
                        if ((comment_mode & RUST_COMMENT_DISPLAY) || (comment_mode & CC_COMMENT_DISPLAY)) {

                            /* add space before comment*/
                            do {
                                WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                            } while (bytes_since_newline-- != 0);
                            ++bytes_since_newline;

                            if (RUST_COMMENT_DISPLAY & comment_mode) {
                                if (str[1] == '!') {
                                    str += 2;
                                }
                                else if (str[1] == '/') {
                                    WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                                    str += str[2] == '!' ? 3 : 2;
                                }
                                else {
                                    ++str;
                                }
                            }
                            else {
                                ++str;
                            }
                            while (*str != '\0') {

                                /* stop when we reach the end of the line */
                                if (str[0] == '\n' || (str[0] == '\r' && str[1] == '\n')) {
                                    if (show_lines) {
                                        WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                                        output_number(newline_count);
                                    }

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
                                    if (show_lines) {
                                        WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                                        output_number(newline_count);
                                    }

                                    WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);

                                    str = continuing_backslash_pos;
                                    ++newline_count;
                                }

                                WriteFile("stdout", stdout, str, 1, NULL, NULL);

                                ++str;
                            }
                            WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);
                        }
                        break;

                    case '*':
                        if (comment_mode & RUST_COMMENT_DISPLAY) {
                            ++result.rust_comment_count;
                            /* add space before comment */
                            do {
                                WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                            } while (bytes_since_newline-- != 0);
                            ++bytes_since_newline;

                            size_t bracket_count = 1;
                            str += str[1] == '!' ? 2 : 1;
                            while (*str != '\0' && bracket_count != 0) {
                                while (str[0] == '/' && str[1] == '*') {
                                    str += str[2] == '!' ? 4 : 3;
                                    ++bracket_count;
                                }

                                if (str[0] == '*' && str[1] == '/') {
                                    if (show_lines) {
                                        WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                                        output_number(newline_count);
                                    }

                                    ++str;
                                    if (str[1] == '\n' || (str[1] == '\r' && str[2] == '\n')) {
                                        WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);

                                        str += str[0] == '\n' ? 1 : 2;
                                        ++newline_count;
                                    }
                                    --bracket_count;
                                }
                                else {
                                    if (str[0] == '\n' || (str[0] == '\r' && str[1] == '\n')) {
                                        if (show_lines) {
                                            /* output a number before the end of the line */
                                            WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                                            output_number(newline_count);
                                        }

                                        WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);

                                        str += str[0] == '\n' ? 0 : 1;
                                        ++newline_count;
                                    }
                                    else {

                                        WriteFile("stdout", stdout, str, 1, NULL, NULL);
                                    }
                                }
                                ++str;
                            }

                            WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);
                        }
                        else if ((comment_mode & C_COMMENT_DISPLAY)) {
                            ++result.c_comment_count;
                            /* add space before comment */
                            do {
                                WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                            } while (bytes_since_newline-- != 0);
                            ++bytes_since_newline;

                            while (*str != '\0') {
                                ++str;
                                if (str[0] == '*' && str[1] == '/') {
                                    if (show_lines) {
                                        WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                                        output_number(newline_count);
                                    }

                                    ++str;
                                    if (str[1] == '\n' || (str[1] == '\r' && str[2] == '\n')) {
                                        str += str[0] == '\n' ? 1 : 2;
                                        ++newline_count;
                                    }
                                    break;
                                }

                                if (str[0] == '\n' || (str[0] == '\r' && str[1] == '\n')) {
                                    if (show_lines) {
                                        /* output a number before the end of the line */
                                        WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                                        output_number(newline_count);
                                    }

                                    WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);

                                    str += str[0] == '\n' ? 0 : 1;
                                    ++newline_count;
                                }
                                else {
                                    WriteFile("stdout", stdout, str, 1, NULL, NULL);
                                }
                            }

                            WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);
                        }
                        break;
                }
                break;

            case '\n':
                bytes_since_newline = 0;
                ++newline_count;
                break;

            case ';':
                ++result.asm_comment_count;
                if ((comment_mode & ASM_COMMENT_DISPLAY)) {
                    /* add space before comment*/
                    do {
                        WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                    } while (bytes_since_newline-- != 0);
                    ++bytes_since_newline;

                    ++str;
                    while (*str != '\0') {
                        if (str[0] == '\n' || (str[0] == '\r' && str[1] == '\n')) {
                            if (show_lines) {
                                WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                                output_number(newline_count);
                            }

                            ++newline_count;
                            break;
                        }

                        WriteFile("stdout", stdout, str, 1, NULL, NULL);
                        ++str;
                    }

                    WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);
                }
                break;

            case '#':
                ++result.python_comment_count;
                if ((comment_mode & PYTHON_COMMENT_DISPLAY)) {
                    /* add space before comment*/
                    do {
                        WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                    } while (bytes_since_newline-- != 0);
                    ++bytes_since_newline;

                    ++str;
                    while (*str != '\0') {
                        if (str[0] == '\n' || (str[0] == '\r' && str[1] == '\n')) {
                            if (show_lines) {
                                WriteFile("stdout", stdout, " ", 1, NULL, NULL);
                                output_number(newline_count);
                            }

                            str += str[0] == '\n' ? 0 : 1;
                            ++newline_count;
                            break;
                        }

                        WriteFile("stdout", stdout, str, 1, NULL, NULL);
                        ++str;
                    }

                    WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);
                }
                break;
        }
        ++str;
        bytes_since_newline += *str == '\t' ? 4 : 1; /* handle tabs */
    }

    return result;
}

static void read_file_comments(char const *filename, comment_display comment_mode, bool show_line_number, bool display_comment_count)
{
    if (comment_mode & AUTO_COMMENT_DISPLAY) {
        comment_mode = get_comment_mode(filename);
    }

    if (comment_mode & ~NO_COMMENT_DISPLAY) {
        WriteFile("stdout", stdout, filename, lstrlenA(filename), NULL, NULL);
        WriteFile("stdout", stdout, ": \r\n", 4, NULL, NULL);
    }
    else {
        return;
    }

    HANDLE file_handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN | FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        error_messagea("Error: could not open file \"", filename, "\"");
    }

    /* get the file size */
    LARGE_INTEGER file_size;
    if (GetFileSizeEx(file_handle, &file_size) == FALSE) {
        error_messagea("Error: could not get the file size of \"", filename, "\\");
    }

    char *file_buffer = HeapAlloc(GetProcessHeap(), 0, file_size.QuadPart + 1);
    file_buffer[file_size.QuadPart] = '\0'; /* add a null terminator */

    /* read the file into the file buffer */
    DWORD bytes_read = 0;
    if (ReadFile(file_handle, file_buffer, file_size.LowPart, &bytes_read, NULL) == FALSE || bytes_read != file_size.QuadPart) {
        error_messagea("Error: could not read ", filename);
    }

    /* process the file and read the comments */
    {
        comment_count count = read_comments(file_buffer, show_line_number, comment_mode);
        if (display_comment_count) {
            if (comment_mode & CC_COMMENT_DISPLAY) {
                WriteFile("stdout", stdout, "c++ style comments: ", 20, NULL, NULL);
                output_number(count.cc_comment_count);
                WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);
            }

            if (comment_mode & C_COMMENT_DISPLAY) {
                WriteFile("stdout", stdout, "c style comments: ", 18, NULL, NULL);
                output_number(count.c_comment_count);
                WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);
            }

            if (comment_mode & RUST_COMMENT_DISPLAY) {
                WriteFile("stdout", stdout, "rust style comments: ", 21, NULL, NULL);
                output_number(count.rust_comment_count);
                WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);
            }

            if (comment_mode & ASM_COMMENT_DISPLAY) {
                WriteFile("stdout", stdout, "asm style comments: ", 20, NULL, NULL);
                output_number(count.asm_comment_count);
                WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);
            }

            if (comment_mode & PYTHON_COMMENT_DISPLAY) {
                WriteFile("stdout", stdout, "python style comments: ", 23, NULL, NULL);
                output_number(count.python_comment_count);
                WriteFile("stdout", stdout, "\r\n", 2, NULL, NULL);
            }
        }
    }
}

typedef struct string
{
    size_t size;
    size_t capacity;
    char *data;
} string_t;

string_t make_string(char const *string)
{
    /* get the length of the string */
    size_t string_length = lstrlenA(string);

    string_t result = {
        .size = string_length,
        .capacity = string_length,
        .data = HeapAlloc(GetProcessHeap(), 0, string_length + 1)
    };

    /* copy the string to result */
    for (char *first = result.data; first != result.data + result.size; ) {
        *first++ = *string++;
    }

    /* null terminator */
    result.data[result.size] = '\0';

    return result;
}

void string_cat(string_t *self, char const *string)
{
    if (string[0] == '\0') return;

    size_t string_length = lstrlenA(string);
    self->size += string_length;
    if (self->size > self->capacity) {
        self->capacity = self->size * 2;
        self->data = HeapReAlloc(GetProcessHeap(), 0, self->data, (self->capacity + 1));
    }
    lstrcatA(self->data, string);
}

void string_free(string_t self)
{
    HeapFree(GetProcessHeap(), 0, self.data);
}

void read_comments_in_directory(char const *input_path, comment_display comment_mode, bool show_line_number, bool display_comment_count)
{
    size_t stack_capacity = 1000;
    size_t stack_size = 1;
    string_t *stack_base = HeapAlloc(GetProcessHeap(), 0, sizeof(string_t) * stack_capacity);
    string_t *stack_ptr = stack_base;

    *stack_ptr++ = make_string(input_path);
    while (stack_ptr != stack_base) {
        string_t path = stack_ptr[-1];
        string_t spec = make_string(path.data);
        string_cat(&spec, "\\*");
        --stack_ptr;
        --stack_size;

        WIN32_FIND_DATAA file_find_data;
        HANDLE find_handle = FindFirstFileA(spec.data, &file_find_data);
        if (find_handle == INVALID_HANDLE_VALUE) {
            string_free(spec);
            string_free(path);
            HeapFree(GetProcessHeap(), 0, stack_base);
            error_messagea("Error: FindFirstFileA failed");
        }

        do {
            if (lstrcmpA(file_find_data.cFileName, ".") != 0 &&
                lstrcmpA(file_find_data.cFileName, "..") != 0) {
                if (file_find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    ++stack_size;
                    if (stack_size > stack_capacity) {
                        stack_capacity = stack_size * 2;
                        stack_base = HeapReAlloc(GetProcessHeap(), 0, stack_base, sizeof(string_t) * stack_capacity);
                        stack_ptr = stack_base + stack_size - 1;
                    }
                    *stack_ptr++ = make_string(path.data);
                    string_cat(stack_ptr - 1, "\\");
                    string_cat(stack_ptr - 1, file_find_data.cFileName);
                }
                else {
                    string_t file_name = make_string(path.data);
                    string_cat(&file_name, "\\");
                    string_cat(&file_name, file_find_data.cFileName);
                    read_file_comments(file_name.data, comment_mode, show_line_number, display_comment_count);
                    string_free(file_name);
                }
            }
        } while (FindNextFileA(find_handle, &file_find_data) != 0);

        if (GetLastError() != ERROR_NO_MORE_FILES) {
            FindClose(find_handle);
            error_messagea("Error: FindNextFileA failed");
        }

        FindClose(find_handle);
        string_free(spec);
        string_free(path);
    }

    HeapFree(GetProcessHeap(), 0, stack_base);
}

void read_comments_in_directory_non_recursive(char const *input_path, comment_display comment_mode, bool show_line_number, bool display_comment_count)
{
    string_t spec = make_string(input_path);
    string_cat(&spec, "\\*");

    WIN32_FIND_DATAA file_find_data;
    HANDLE find_handle = FindFirstFileA(spec.data, &file_find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        string_free(spec);
        error_messagea("Error: FindFirstFileA failed");
    }

    string_t file_name = make_string(input_path);
    string_cat(&file_name, "\\");
    do {
        if (lstrcmpA(file_find_data.cFileName, ".") != 0 &&
            lstrcmpA(file_find_data.cFileName, "..") != 0) {
            if (file_find_data.dwFileAttributes & ~FILE_ATTRIBUTE_DIRECTORY) {
                string_cat(&file_name, file_find_data.cFileName);
                read_file_comments(file_name.data, comment_mode, show_line_number, display_comment_count);
                file_name.data[spec.size - 1] = '\0';
                file_name.size = spec.size - 1;
            }
        }
    } while (FindNextFileA(find_handle, &file_find_data) != 0);

    FindClose(find_handle);
    string_free(spec);
    string_free(file_name);
}

void __cdecl mainCRTStartup(void)
{
    static char const *help_message = "Usage: comments [--help] [-r false or true or --recursive= false or true] [-l or --line] [-c or --count] [-nl or --no_line] [-e [mode] or --enable=[mode]] [-m [mode] or --mode=[mode]] [-d [mode] or --disable=[mode]] [--display_comment_count or -dcc] [--hide_comment_count -hcc] [file1 ...]\n\
                                         Flags: \n\
                                        --help: displays this message \n\
                                        -r or --recursive=: using this with true enables recursive directory searching and using with false disables recursive directory search\n\
                                        -l or --line(disabled by default): makes it so the program shows the line number of each comment\n\
                                        -nl or --no_line: has the oppsite effect of -l\n\
                                        -e [mode] or --enable=[mode]: enables the comment mode to [mode] for example -e asm enables asm comments\n\
                                        -m [mode] or --mode=[mode]: sets the comment mode  to [mode] for example -m asm only allows only asm comments\n\
                                        -d [mode] or --disable=[mode]: disables the comment mode to [mode] for example -d asm disables asm comments\n\
                                        [mode](the default mode auto): the different modes are \n\
                                        c++ style comments //(cc, cxx, cpp), \n\
                                        c style comments /**/ (c), \n\
                                        python style comments (py), \n\
                                        asm style comments ;(asm), \n\
                                        c and c++ style comments /**/ //(c|c++), \n\
                                        rust style comments which enables rust style comments /*/* comments can be nested */*/ // /// //!(rs), \n\
                                        auto which detects the comment style based on file extension(auto), \n\
                                        and all which enables all the available comment styles(all) \n\
                                        -dcc or --display_comment_count(enabled by defualt): displays the number of comments found \n\
                                        -hcc or --hides_comment_count: hides the number of comments found \n\
                                        ";
    stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    stderr = GetStdHandle(STD_ERROR_HANDLE);

    /* get command line args */
    int argc;
    char **argv = CommandLineToArgvA(GetCommandLineA(), &argc) + 1;
    --argc;

    bool show_lines = false;
    bool recursive_directory_search = false;
    bool display_comment_count = true;
    comment_display comment_mode = AUTO_COMMENT_DISPLAY;
    DWORD file_type = -1;

    /* this makes it easier to add flags */
#define FIND_ARG(op)                                                        \
    if (!lstrcmpiA(argv[i], "cc") || !lstrcmpiA(argv[i], "cxx")             \
        || !lstrcmpiA(argv[i], "cpp")) {                                    \
        comment_mode op CC_COMMENT_DISPLAY;                                 \
    } else if (!lstrcmpiA(argv[i], "c")) {                                  \
        comment_mode op C_COMMENT_DISPLAY;                                  \
    } else if (!lstrcmpiA(argv[i], "asm")) {                                \
        comment_mode op ASM_COMMENT_DISPLAY;                                \
    } else if (!lstrcmpiA(argv[i], "c|c++")) {                              \
        comment_mode op C_AND_CC_COMMENT_DISPLAY;                           \
    } else if (!lstrcmpiA(argv[i], "auto")) {                               \
        comment_mode op AUTO_COMMENT_DISPLAY;                               \
    } else if(!lstrcmpiA(argv[i], "py")) {                                  \
        comment_mode op PYTHON_COMMENT_DISPLAY;                             \
    } else if(!lstrcmpiA(argv[i], "rs")) {                                  \
        comment_mode op (RUST_COMMENT_DISPLAY);                             \
    } else if (!lstrcmpiA(argv[i], "all")) {                                \
        comment_mode op ALL_COMMENT_DISPLAY;                                \
    } else {                                                                \
        error_messagea("Error: invalid arguments\n", help_message);         \
    }                                                                       \

    /* parse command line args */
    for (int i = 0; i < argc; ++i) {
        if (!lstrcmpA(argv[i], "--line") || !lstrcmpA(argv[i], "-l")) {
            show_lines = true;
        }
        else if (i + 1 < argc && !lstrcmpA(argv[i], "-r")) {
            ++i;
            if (!lstrcmpiA(argv[i], "true")) {
                recursive_directory_search = true;
            }
            else if (!lstrcmpiA(argv[i], "false")) {
                recursive_directory_search = false;
            }
            else {
                error_messagea("Error: invalid arguments\n", help_message);
            }
        }
        else if (argv[i][0] == '-' && argv[i][1] == '-'
            && argv[i][2] == 'r' && argv[i][3] == 'e'
            && argv[i][4] == 'c' && argv[i][5] == 'u'
            && argv[i][6] == 'r' && argv[i][7] == 's'
            && argv[i][8] == 'i' && argv[i][9] == 'v'
            && argv[i][10] == 'e' && argv[i][11] == '=') {
            argv[i] += 12;
            if (!lstrcmpiA(argv[i], "true")) {
                recursive_directory_search = true;
            }
            else if (!lstrcmpiA(argv[i], "false")) {
                recursive_directory_search = false;
            }
            else {
                error_messagea("Error: invalid arguments\n", help_message);
            }
        }
        else if (!lstrcmpA(argv[i], "--no_line") || !lstrcmpA(argv[i], "-nl")) {
            show_lines = false;
        }
        else if (!lstrcmpA(argv[i], "--display_comment_count") || !lstrcmpA(argv[i], "-dcc")) {
            display_comment_count = true;
        }
        else if (!lstrcmpA(argv[i], "--hide_comment_count") || !lstrcmpA(argv[i], "-hcc")) {
            display_comment_count = false;
        }
        else if (argv[i][0] == '-' && argv[i][1] == '-'
            && argv[i][2] == 'm' && argv[i][3] == 'o'
            && argv[i][4] == 'd' && argv[i][5] == 'e'
            && argv[i][6] == '=') {
            argv[i] += 7;
            FIND_ARG(= );
        }
        else if (i + 1 < argc && !lstrcmpA(argv[i], "-m")) {
            ++i;
            FIND_ARG(= );
        }
        else if (argv[i][0] == '-' && argv[i][1] == '-'
            && argv[i][2] == 'm' && argv[i][3] == 'o'
            && argv[i][4] == 'd' && argv[i][5] == 'e'
            && argv[i][6] == '=') {
            argv[i] += 7;
            FIND_ARG(= );
        }
        else if (i + 1 < argc && !lstrcmpA(argv[i], "-e")) {
            ++i;
            FIND_ARG(|= );
        }
        else if (argv[i][0] == '-' && argv[i][1] == '-'
            && argv[i][2] == 'd' && argv[i][3] == 'i'
            && argv[i][4] == 's' && argv[i][5] == 'a'
            && argv[i][6] == 'b' && argv[i][7] == 'l'
            && argv[i][8] == 'e' && argv[i][9] == '=') {
            argv[i] += 10;
            FIND_ARG(&= ~);
        }
        else if (i + 1 < argc && !lstrcmpA(argv[i], "-d")) {
            ++i;
            FIND_ARG(&= ~);
        }
        else if (!lstrcmpiA(argv[i], "--help")) {
            WriteFile("stdout", stdout, help_message, lstrlenA(help_message), NULL, NULL);
        }
        else if (((file_type = GetFileAttributesA(argv[i])) & ~FILE_ATTRIBUTE_DIRECTORY) && file_type != INVALID_FILE_ATTRIBUTES) {
            read_file_comments(argv[i], comment_mode, show_lines, display_comment_count);
        }
        else if (file_type != INVALID_FILE_ATTRIBUTES && (file_type & FILE_ATTRIBUTE_DIRECTORY)) {
            if (recursive_directory_search) {
                read_comments_in_directory(argv[i], comment_mode, show_lines, display_comment_count);
            }
            else {
                read_comments_in_directory_non_recursive(argv[i], comment_mode, show_lines, display_comment_count);
            }
        }
        else {
            error_messagea("Error: invalid arguments\n", help_message);
        }
    }

    /* cleanup */
    LocalFree(argv - 1);

    ExitProcess(0);
}