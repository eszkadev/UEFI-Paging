/*
Author: Szymon Kłos
14-01-2016
*/

#include <Uefi.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/PrintLib.h>

#define DIRECTORY_START 0x4020000
#define TABLES 1024
#define PAGING_START_ADDR 0

#define IDT_PAGE_FAULT_OFFSET 0x0E

// RW, SUPERVISOR, NOT PRESENT
#define EMPTY_ENTRY 2
// RW, SUPERVISOR, PRESENT
#define FLAGS 3

#define FLAG_PRESENT 1

/*

--------- < DIRECTORY_START + 1024 ENTRIES * 4 BYTES + 1024 TABLES * 1024 ENTRIES * 4 BYTES
 TABLE  |
 1024   |
---------
  ...   |
---------
TABLE 2 |
---------
TABLE 1 |
--------- < DIRECTORY_START + 1024 ENTRIES * 4 BYTES
	|
 TABLE  |
  DIR	|
	|
--------- < DIRECTORY_START

*/

extern void SetPageDirectory(UINT32*);
extern void EnablePaging();
extern void* GetIDTR();
extern UINT32 GetCR2();
extern void PageFaultHandler();

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

#define SIZE 1024
CHAR16 String[SIZE];
EFI_SYSTEM_TABLE* ST;

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
	else if(StrnCmp(String, L"writeb", 5) == 0)
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

VOID MapAddress( IN UINT32 linear, IN UINT32 phys, IN UINT16 flags, IN UINT32* page_directory )
{
	UINT32 PDI = linear >> 22;

	if(page_directory[PDI] != EMPTY_ENTRY)
	{
		UINT32* table = (UINT32*)(page_directory[PDI] & 0xfffff000);
		UINT32 PTI = ( linear >> 12 ) & 0x03FF;
		table[PTI] = phys | flags;
	}
}

VOID MapAddressIdent( IN UINT32 address, IN UINT16 flags, IN UINT32* table, IN UINT32* page_directory )
{
	UINT32 PDI = address >> 22;

	if(page_directory[PDI] == EMPTY_ENTRY)
	{
		page_directory[PDI] = (UINT32)(table) | flags;

		UINT32 i;
		for(i = 0; i < 1024; i++)
		{
			table[i] = address | flags;
			address = address + 4096;
		}
	}
}

VOID
SetNotPresent( IN UINT32 linear, IN UINT32* page_directory )
{
	UINT32 PDI = linear >> 22;

	if(page_directory[PDI] != EMPTY_ENTRY)
	{
		UINT32* page = (UINT32*)(page_directory[PDI] & 0xfffff000);
		UINT32 PTI = ( linear >> 12 ) & 0x03FF;

		page[PTI] &= ~((UINT32)FLAG_PRESENT);
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

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"adres (z) {hex}: ");
	ReadWord(SystemTable, SIZE, String);
	address1 = StrHexToUint64(String);

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\nadres (na) {hex}: ");
	ReadWord(SystemTable, SIZE, String);
	address2 = StrHexToUint64(String);

	UnicodeSPrint(String, SIZE, L"\nMapowanie z %x na %x \r\n", address1, address2);
	SystemTable->ConOut->OutputString(SystemTable->ConOut, String);

	MapAddress(address1, address2, FLAGS, (UINT32*)DIRECTORY_START);
}

VOID Read( IN EFI_SYSTEM_TABLE *SystemTable )
{
	UINT32 address = 0;
	UINT32 count = 0;

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"adres {hex}: ");
	ReadWord(SystemTable, SIZE, String);
	address = StrHexToUint64(String);

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\nilosc bajtow {dec}: ");
	ReadWord(SystemTable, SIZE, String);
	count = StrDecimalToUint64(String);

	UnicodeSPrint(String, SIZE, L"\nCzytanie z %x, %d bajtow:\r\n", address, count);
	SystemTable->ConOut->OutputString(SystemTable->ConOut, String);

	PrintBytes(SystemTable, (UINT8*)address, count);
}

VOID NotPresent( IN EFI_SYSTEM_TABLE *SystemTable )
{
	UINT32 address = 0;

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"adres {hex}: ");
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

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"adres {hex}: ");
	ReadWord(SystemTable, SIZE, String);
	address = StrHexToUint64(String);

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\nilosc bajtow {dec}: ");
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

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Wpisano:\n\r");

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

VOID SetIDTEntryOffset( IN EFI_SYSTEM_TABLE *SystemTable, IN UINT64* address, IN UINT32* pointer )
{
	UINT16 off1 = (UINT32)pointer;
	UINT16 off2 = (UINT32)pointer >> 16;

	UINT64 entry = *address;
	entry = entry & (~((UINT64)0xffff));
	entry = entry & (~(((UINT64)0xffff)<<48));

	entry |= off1;
	entry |= (UINT64)off2 << 48;

	*address = entry;
}

VOID SetPagePresent()
{
	UINT32 cr2 = GetCR2();
	UnicodeSPrint( String, SIZE, L"PAGE FAULT!!! address = %x \n\r", cr2);
	ST->ConOut->OutputString(ST->ConOut, String);

	UINT32* page_directory = (UINT32*)DIRECTORY_START;
	UINT32 PDI = cr2 >> 22;

	if(page_directory[PDI] != EMPTY_ENTRY)
	{
		UINT32* page = (UINT32*)(page_directory[PDI] & 0xfffff000);
		UINT32 PTI = ( cr2 >> 12 ) & 0x03FF;

		page[PTI] |= FLAG_PRESENT;
	}
}

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
	ST = SystemTable;
	SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

	CHAR16 CommandString[SIZE];
	UINT32 i;

	UINT32* pointer = 0;
	UINT32* page_directory = (UINT32*)DIRECTORY_START;

	// set each entry to not present
	for(i = 0; i < 1024; i++)
	{
	    page_directory[i] = EMPTY_ENTRY;
	}
 
	pointer = (UINT32*)(DIRECTORY_START + 4096);

	for(i = 0; i < 1024*TABLES; i++)
	{
		pointer[i] = EMPTY_ENTRY;
	}

	// map memory
	UINT32 linear = PAGING_START_ADDR;
	linear = (linear & 0xFFFFF000) - ((( linear >> 12) & 0x03FF)*0x1000);

	pointer = (UINT32*)DIRECTORY_START;
	for(i = 0; i < TABLES; i++)
	{
		pointer += 1024;
		MapAddressIdent(linear, FLAGS, pointer, page_directory);
		linear += 4096*1024;
	}

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Paging x86 \n\r\n\rTurn on paging... ");

	// turn on paging
	SetPageDirectory(page_directory);
	EnablePaging();

	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"OK \n\r\n\r");

	// show help
	PrintHelp(SystemTable);

	// get idt table info
	UINT8* idtr = GetIDTR();
	idtr += 2; // length field
	UINT32 idt = *((UINT32*)idtr);

	// set new page fault handler
	UINT64* idt_entry = (UINT64*)idt;
	PrintIDTEntry(SystemTable, idt_entry + IDT_PAGE_FAULT_OFFSET);

	SetIDTEntryOffset(SystemTable, idt_entry + IDT_PAGE_FAULT_OFFSET, (UINT32*)PageFaultHandler);

	PrintIDTEntry(SystemTable, idt_entry + IDT_PAGE_FAULT_OFFSET);

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

	return EFI_SUCCESS;
}
