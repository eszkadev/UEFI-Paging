/*
Author: Szymon Kłos
14-01-2016
*/

#ifndef _MENU_H_
#define _MENU_H_

#include <Uefi.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/PrintLib.h>

#define SIZE 1024
CHAR16 String[SIZE];

VOID PrintHelp( IN EFI_SYSTEM_TABLE *SystemTable );
UINT8 ParseCommand( IN CHAR16* String );
VOID ReadWord( IN EFI_SYSTEM_TABLE *SystemTable, IN UINT16 MaxSize, OUT CHAR16* Word );
VOID PrintBytes( IN EFI_SYSTEM_TABLE *SystemTable, UINT8* address, UINT32 count );
VOID Map( IN EFI_SYSTEM_TABLE *SystemTable );
VOID Read( IN EFI_SYSTEM_TABLE *SystemTable );
VOID NotPresent( IN EFI_SYSTEM_TABLE *SystemTable );
VOID WriteB( IN EFI_SYSTEM_TABLE *SystemTable );
VOID PrintIDTEntry( IN EFI_SYSTEM_TABLE *SystemTable, IN UINT64* address );

VOID Menu(IN EFI_SYSTEM_TABLE *SystemTable);

#endif
