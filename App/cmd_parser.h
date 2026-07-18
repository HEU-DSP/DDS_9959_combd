/**
 ******************************************************************************
 * @file    cmd_parser.h
 * @brief   Command Parser — Skeleton (Phase 3)
 ******************************************************************************
 */

#ifndef __CMD_PARSER_H__
#define __CMD_PARSER_H__

#include <stdint.h>

void CmdParser_Init(void);
int  CmdParser_Parse(const char *line);
void CmdParser_PrintHelp(void);

#endif /* __CMD_PARSER_H__ */
