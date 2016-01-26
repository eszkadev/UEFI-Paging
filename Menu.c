/*
Author: Szymon Kłos
14-01-2016
*/

#include "Menu.h"
#include "Paging.h"

enum
{
	NO_COMMAND,
	HELP,
	WRITEB,
	WRITES,
	READ,
	MAP,
	NOTPRESENT,
	EXIT
};

VOID PrintHelp( IN EFI_SYSTEM_TABLE *SystemTable )
{
	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"help - show help \n\r");
	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"write - write bytes to memory {hex}\n\r");
	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"read - read bytes from memory \n\r");
	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"map - map logical address \n\r");
	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"notpresent - set pae as not present\n\r");
	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"exit\n\r");
	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\n\r");
}

UINT8 ParseCommand( IN CHAR16* String )
{
	if(StrnCmp(String, L"help", 4) == 0)
		return HELP;
	else if(StrnCmp(String, L"write", 5) == 0)
		return WRITEB;
	else if(StrnCmp(String, L"read", 4) == 0)
		return READ;
	else if(StrnCmp(String, L"map", 3) == 0)
		return MAP;
	else if(StrnCmp(String, L"exit", 4) == 0)
		return EXIT;
	else if(StrnCmp(String, L"notpresent", 10) == 0)
		return NOTPRESENT;

	return NO_COMMAND;
}

VOID ReadWord( IN EFI_SYSTEM_TABLE *SystemTable, IN UINT16 MaxSize, OUT CHAR16* Word )
{
	EFI_INPUT_KEY Key;
	UINT16 Index = 0;
	CHAR16 string[2];
	string[0] = 0;

	while(string[0] != '\r' && Index < MaxSize - 1)
	{
		while(SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key) != EFI_SUCCESS);

		string[0] = Key.UnicodeChar;
		string[1] = 0;
		SystemTable->ConOut->OutputString(SystemTable->ConOut, string);

		Word[Index++] = string[0];
	}
}

VOID PrintBytes( IN EFI_SYSTEM_TABLE *SystemTable, UINT8* address, UINT32 count )
{
	UINT32 i, j, left;

	for(i = 0; i < count;)
	{
		left = count - i;
		
		for(j = 0; j < left && j < 10; j++)
		{
			UnicodeSPrint(String, SIZE, L"%2x ", *address);
			SystemTable->ConOut->OutputString(SystemTable->ConOut, String);

			address++;
			i++;
		}

		address -= (left > 10) ? 10 : left;
		i -= (left > 10) ? 10 : left;

		SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\t");

		for(j = 0; j < left && j < 10; j++)
		{
			if((*address >= 'a' && *address <= 'z') || (*address >= 'A' && *address <= 'Z'))
			{
				UnicodeSPrint(String, SIZE, L"%c ", *address);
				SystemTable->ConOut->OutputString(SystemTable->ConOut, String);
			}
			else
			{
				SystemTable->ConOut->OutputString(SystemTable->ConOut, L". ");
			}

			address++;
			i++;
		}

		SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\n\r");
	}

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\n\r");
}

VOID Map( IN EFI_SYSTEM_TABLE *SystemTable )
{
	UINT32 address1 = 0;
	UINT32 address2 = 0;

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"adres (linear) {hex}: ");
	ReadWord(SystemTable, SIZE, String);
	address1 = StrHexToUint64(String);

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\nadres (phys) {hex}: ");
	ReadWord(SystemTable, SIZE, String);
	address2 = StrHexToUint64(String);

	UnicodeSPrint(String, SIZE, L"\nMapping: %x -> %x \r\n", address1, address2);
	SystemTable->ConOut->OutputString(SystemTable->ConOut, String);

	MapAddress(address1, address2, FLAGS, (UINT32*)DIRECTORY_START);
}

VOID Read( IN EFI_SYSTEM_TABLE *SystemTable )
{
	UINT32 address = 0;
	UINT32 count = 0;

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"address {hex}: ");
	ReadWord(SystemTable, SIZE, String);
	address = StrHexToUint64(String);

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\nbytes count {dec}: ");
	ReadWord(SystemTable, SIZE, String);
	count = StrDecimalToUint64(String);

	UnicodeSPrint(String, SIZE, L"\nReading form %x, %d bytes:\r\n", address, count);
	SystemTable->ConOut->OutputString(SystemTable->ConOut, String);

	PrintBytes(SystemTable, (UINT8*)address, count);
}

VOID NotPresent( IN EFI_SYSTEM_TABLE *SystemTable )
{
	UINT32 address = 0;

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"address {hex}: ");
	ReadWord(SystemTable, SIZE, String);
	address = StrHexToUint64(String);

	SetNotPresent(address, (UINT32*)DIRECTORY_START);

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\n\r");
}

VOID WriteB( IN EFI_SYSTEM_TABLE *SystemTable )
{
	UINT32 address = 0;
	UINT32 count = 0;
	UINT32 i;
	UINT8 x;

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"address {hex}: ");
	ReadWord(SystemTable, SIZE, String);
	address = StrHexToUint64(String);

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\nbytes count {dec}: ");
	ReadWord(SystemTable, SIZE, String);
	count = StrDecimalToUint64(String);

	for(i = 0; i < count; i++)
	{
		UnicodeSPrint(String, SIZE, L"\n%x: ", address);
		SystemTable->ConOut->OutputString(SystemTable->ConOut, String);
		ReadWord(SystemTable, SIZE, String);
		x = StrHexToUint64(String);
		*(UINT8*)address = x;
		address++;
	}

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Bytes written:\n\r");

	PrintBytes(SystemTable, (UINT8*)(address - count), count);
}

VOID PrintIDTEntry( IN EFI_SYSTEM_TABLE *SystemTable, IN UINT64* address )
{
	UINT16 off1 = (*address) & 0xffff;
	UINT16 off2 = (*address) >> 48;
	UINT32 offset = off1 + (off2 << 16);
	UINT8 zero = ((*address) >> 32) & 0xff;
	UINT16 selector = ((*address) >> 16) & 0xffff;
	UINT8 type = ((*address) >> 40) & 0xff;

	UnicodeSPrint( String, SIZE, L"off = %x, zero = %x, type = %x, selector = %x \n\r", offset, zero, type, selector );
	SystemTable->ConOut->OutputString(SystemTable->ConOut, String);
}

VOID Menu(IN EFI_SYSTEM_TABLE *SystemTable)
{
	CHAR16 CommandString[SIZE];

	// show help
	PrintHelp(SystemTable);

	BOOLEAN done = FALSE;
	while(!done)
	{
		SystemTable->ConOut->OutputString(SystemTable->ConOut, L"> ");

		ReadWord(SystemTable, SIZE, CommandString);

		UINT8 command = ParseCommand(CommandString);

		UnicodeSPrint( String, SIZE, L"\n" );
		SystemTable->ConOut->OutputString(SystemTable->ConOut, String);

		if(command == EXIT)
		{
			done = TRUE;
		}
		else if(command == HELP)
		{
			PrintHelp(SystemTable);
		}
		else if(command == MAP)
		{
			Map(SystemTable);
		}
		else if(command == READ)
		{
			Read(SystemTable);
		}
		else if(command == WRITEB)
		{
			WriteB(SystemTable);
		}
		else if(command == NOTPRESENT)
		{
			NotPresent(SystemTable);
		}
	}
}
