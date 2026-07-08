/*
 * SquirrelDB - 一个简单的 SQL 数据库（教学演示项目）
 *
 * 基于 cstack 的教程 "Let's Build a Simple Database"
 * 当前阶段：实现 REPL 命令行界面，支持 .exit、insert、select 命令
 */

#include <string.h>   /* 字符串操作，如 strcmp、strncmp */
#include <stdio.h>    /* 标准输入输出，如 printf、getline */
#include <stdlib.h>   /* 内存管理，如 malloc、free、exit */
#include <unistd.h>   /* POSIX 系统调用 */
#include <stdbool.h>  /* bool 类型 */
#include <stdint.h>   /* 定长整数类型，如 uint32_t */


/* ===================== 宏定义 ===================== */

#define COLUMN_USERNAME_SIZE 32   /* 用户名字段最大字节数 */
#define COLUMN_EMAIL_SIZE 255     /* 邮箱字段最大字节数 */


/* ===================== 数据结构定义 ===================== */

/**
 * Row - 数据行结构
 * 对应数据库表中的一行记录
 */
typedef struct 
{
    uint32_t id;                        /* 行 ID（主键） */
    char username[COLUMN_USERNAME_SIZE]; /* 用户名 */
    char email[COLUMN_EMAIL_SIZE];       /* 邮箱地址 */
} Row;


/**
 * MetaCommandResult - 元命令执行结果枚举
 * 元命令是以 '.' 开头的特殊命令（如 .exit）
 */
typedef enum {
    META_COMMAND_SUCCESS,                /* 元命令执行成功 */
    META_COMMAND_UNRECOGNIZED_COMMAND,   /* 无法识别的元命令 */
} MetaCommandResult;


/**
 * PrepareResult - SQL 语句预编译结果枚举
 */
typedef enum {
    PREPARE_SUCCESS,                    /* 预编译成功 */
    PREPARE_SYNTAX_ERROR,               /* 语法错误 */
    PREPARE_UNRECOGNIZED_STATEMENT,     /* 无法识别的语句 */
} PrepareResult;


/**
 * InputBuffer - 输入缓冲区结构
 * 用于存储用户从命令行输入的一行内容
 */
typedef struct
{
    char *buffer;          /* 缓冲区指针，存储输入的字符串 */
    size_t buffer_length;  /* 缓冲区已分配的总大小 */
    ssize_t input_length;  /* 输入字符串的实际长度 */
} InputBuffer;


/**
 * StatementType - SQL 语句类型枚举
 */
typedef enum 
{
    STATEMENT_INSERT,  /* INSERT 语句 */
    STATEMENT_SELECT,  /* SELECT 语句 */
} StatementType;


/**
 * Statement - 编译后的 SQL 语句结构
 * 预编译阶段将原始字符串解析为结构化表示
 */
typedef struct
{
    StatementType type;            /* 语句类型 */
    Row row_to_insert;             /* INSERT 语句携带的行数据（仅 INSERT 使用） */
} Statement;


/* ===================== 工具宏 ===================== */

/**
 * 计算结构体中某个属性的字节偏移量/大小
 * 通过将空指针强制转换为结构体指针，再访问属性来获取 sizeof
 */
#define size_of_attribute(Struct, Attribute) sizeof (((Struct*)0)->Attribute)

/* 预计算 Row 结构体字段的大小常量 */
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);


/* ===================== 输入缓冲区操作 ===================== */

/**
 * new_input_buffer - 创建并初始化一个新的输入缓冲区
 *
 * 分配 InputBuffer 结构体内存，并将各字段初始化为零值。
 * 注意：buffer 会在首次调用 getline 时由系统自动分配。
 *
 * 返回值：指向新创建的 InputBuffer 的指针
 */
InputBuffer* new_input_buffer() {
    InputBuffer* input_buffer = (InputBuffer*)malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;
    return input_buffer;
}


/**
 * close_input_buffer - 释放输入缓冲区及其关联资源
 *
 * @param input_buffer: 要释放的 InputBuffer 指针
 *                      函数内部会将其 buffer 和自身都 free 掉，
 *                      调用后该指针不再可用。
 */
void close_input_buffer(InputBuffer *input_buffer) {
    free(input_buffer->buffer);   /* 释放 getline 分配的字符串内存 */
    free(input_buffer);           /* 释放结构体自身 */
}


/* ===================== REPL 界面 ===================== */

/**
 * print_prompt - 打印命令行提示符
 *
 * 向标准输出打印 "db -> "，提示用户输入命令
 */
void print_prompt()
{
    printf("db -> ");
}


/**
 * read_input - 从标准输入读取一行用户输入
 *
 * 使用 POSIX 的 getline 函数读取整行（自动分配足够大的缓冲区）。
 * 读取后去除末尾换行符，将其替换为字符串结束符 '\0'。
 *
 * @param input_buffer: 用于存储输入内容的 InputBuffer 结构
 *                      如果 buffer 为空，getline 会自行 malloc 分配空间
 */
void read_input(InputBuffer *input_buffer) 
{
    /* getline 读取一行，第二个参数传入 buffer_length 的地址 */
    ssize_t bytes_read = getline(&(input_buffer->buffer), 
                                 &(input_buffer->buffer_length), 
                                 stdin);

    if (bytes_read <= 0) 
    {
        printf("Error reading input\n");
        exit(EXIT_FAILURE);
    }

    /* 去除末尾换行符，将换行符替换为字符串结束符 */
    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = '\0';
}


/* ===================== 元命令处理 ===================== */

/**
 * do_meta_command - 处理元命令（以 '.' 开头的命令）
 *
 * 当前支持的元命令：
 *   - .exit : 退出数据库程序
 *
 * @param input_buffer: 包含用户输入的缓冲区
 * @return: MetaCommandResult 枚举值
 */
MetaCommandResult do_meta_command(InputBuffer* input_buffer) {
    if (strcmp(input_buffer->buffer, ".exit") == 0)
    {
        /* .exit 命令：清理资源并退出程序 */
        close_input_buffer(input_buffer);
        printf("Good bye! \n");
        exit(EXIT_SUCCESS);
    }
    /* 其他无法识别的元命令 */
    return META_COMMAND_UNRECOGNIZED_COMMAND;
}


/* ===================== SQL 语句预编译 ===================== */

/**
 * prepare_statement - 预编译 SQL 语句
 *
 * 将用户输入的原始字符串解析为结构化的 Statement 对象。
 * 此阶段只做语句类型识别，不执行实际的数据操作。
 *
 * @param input_buffer: 包含用户输入字符串的缓冲区
 * @param statement:    输出参数，存储编译后的语句
 * @return: PrepareResult 枚举值，表示预编译结果
 */
PrepareResult prepare_statement(InputBuffer *input_buffer,
                                Statement *statement
) {
    /* 检查是否为 INSERT 语句（前 6 个字符匹配 "insert"） */
    if (strncmp(input_buffer->buffer, "insert", 6) == 0) 
    {
        statement->type = STATEMENT_INSERT;
        return PREPARE_SUCCESS;
    }
    
    /* 检查是否为 SELECT 语句 */
    if (strcmp(input_buffer->buffer, "select") == 0) 
    {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }

    /* 无法识别的语句 */
    return PREPARE_UNRECOGNIZED_STATEMENT;
}


/* ===================== SQL 语句执行 ===================== */

/**
 * execute_statement - 执行编译后的 SQL 语句
 *
 * 根据 Statement 类型执行对应的数据库操作。
 * 当前为桩代码（stub），尚未实现实际功能。
 *
 * @param statement: 要执行的已编译语句
 */
void execute_statement(Statement *statement) {
    /* TODO: 实现 INSERT 和 SELECT 的实际执行逻辑 */
    /* 需要实现：表存储、分页、B-tree 索引、文件持久化等 */
}


/* ===================== 主函数 ===================== */

/**
 * main - 数据库程序入口
 *
 * 实现 REPL（Read-Eval-Print Loop）主循环：
 *   1. 打印提示符 "db ->"
 *   2. 读取用户输入
 *   3. 根据输入类型分发处理：
 *      - '.' 开头的命令 -> 交给元命令处理器
 *      - 其他输入 -> 尝试作为 SQL 语句预编译和执行
 *   4. 回到步骤 1，直到用户输入 .exit 退出
 *
 * @param argc: 命令行参数个数（当前未使用）
 * @param argv: 命令行参数数组（当前未使用）
 * @return: 程序退出码
 */
int main(int argc, char *argv[]) {

    /* 初始化输入缓冲区 */
    InputBuffer *input_buffer = new_input_buffer();

    /* REPL 主循环 */
    while (true) {
        /* 1. 打印提示符 */
        print_prompt();

        /* 2. 读取用户输入 */
        read_input(input_buffer);

        /* 3. 判断输入类型并处理 */
        if (input_buffer->buffer[0] == '.')
        {
            /* 元命令处理分支 */
            switch (do_meta_command(input_buffer))
            {
            case META_COMMAND_SUCCESS:
                continue;
            case META_COMMAND_UNRECOGNIZED_COMMAND:
                printf("Unrecognized command '%s'.\n", input_buffer->buffer);
                continue;
            }
        }
        
        /* SQL 语句处理分支 */
        Statement statement;
        switch (prepare_statement(input_buffer, &statement))
        {
            case PREPARE_SUCCESS:
                break;  /* 预编译成功，继续执行 */
            case PREPARE_UNRECOGNIZED_STATEMENT:
                printf("Unrecognized keyword at start of '%s'.\n", 
                       input_buffer->buffer);
                continue;
            /* 暂未处理 PREPARE_SYNTAX_ERROR */
        }

        /* 4. 执行语句 */
        execute_statement(&statement);
        printf("Executed.\n");
    }
}
