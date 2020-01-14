
#ifdef __cplusplus
extern "C"{
#endif

#include <ntifs.h>
#include <ntddk.h>

#include "ActiveWindow.h"
#include "SSDT.h"
#include "Common.h"
#include "Win10_1703_x64.h"

	typedef ULONG_PTR(__fastcall *pfNtUserGetForegroundWindow)();

	typedef HANDLE(__fastcall *pfNtUserQueryWindow)(
		IN HANDLE hwnd,
		IN ULONG WindowInfo);


	extern ULONG g_RelatedProcessId;
	extern KEVENT g_ThreadTerminalEvent;

	BOOLEAN	g_IsActive = FALSE;


	pfNtUserGetForegroundWindow NtUserGetForegroundWindow = NULL;
	pfNtUserQueryWindow NtUserQueryWindow = NULL;

	ULONGLONG GetGuiThread(PEPROCESS eprocess)
	{
		typedef struct _MY_EPROCESS_WIN10_1703_X64_ *PMY_EPROCESS, MY_EPROCESS;
		typedef struct _MY_ETHREAD_WIN10_1703_X64_ *PMY_ETHREAD, MY_ETHREAD;
		typedef struct _MY_KTHREAD_WIN10_1703_X64_ *PMY_KTHREAD, MY_KTHREAD;

		PMY_EPROCESS myEprocess;

		myEprocess = (PMY_EPROCESS)eprocess;

		PLIST_ENTRY plist;

		for (plist = myEprocess->ThreadListHead.Flink; plist != &myEprocess->ThreadListHead; plist = plist->Flink)
		{
			PMY_ETHREAD myEthread = CONTAINING_RECORD(plist, MY_ETHREAD, ThreadListEntry);

			PMY_KTHREAD myKthread = (PMY_KTHREAD)myEthread;

			if (myKthread->Win32Thread != 0)
			{
				return myKthread->Win32Thread;
			}
		}

		return 0;
	}


	// ���ڴ˺�����hook��kbdclass read complete�в��ɵ��ã��ĵ����̵߳���
	BOOLEAN IsRelatedWindowActive()
	{
		if (g_RelatedProcessId == 0)
		{
			return FALSE;
		}

		if (!NtUserGetForegroundWindow)
		{
			NtUserGetForegroundWindow = (pfNtUserGetForegroundWindow)GetShadowSSDTProcAddr(0x3f);
		}
		if (!NtUserQueryWindow)
		{
			NtUserQueryWindow = (pfNtUserQueryWindow)GetShadowSSDTProcAddr(0x13);
		}

		PWCHAR relateName = NULL;

		GetProcessNameByPid(g_RelatedProcessId, &relateName);

		// ������д���ˣ��Ժ����ͨ��Ӧ�ò㴫������Ϣ
		if (wcscmp(relateName, L"notepad.exe") == 0)
		{
			ExFreePool(relateName);
			PEPROCESS relProcEProcess = NULL;
			NTSTATUS status = PsLookupProcessByProcessId((HANDLE)g_RelatedProcessId, &relProcEProcess);
			if (!NT_SUCCESS(status))
			{
				return FALSE;
			}

			PRKAPC_STATE apcState;
			apcState = (PRKAPC_STATE)ExAllocatePoolWithTag(NonPagedPool, sizeof(KAPC_STATE), KBDTAG);

			KeStackAttachProcess(relProcEProcess, apcState);

			PETHREAD currentEThread = KeGetCurrentThread();


			typedef struct _MY_EPROCESS_WIN10_1703_X64_ *PMY_EPROCESS, MY_EPROCESS;
			typedef struct _MY_ETHREAD_WIN10_1703_X64_ *PMY_ETHREAD, MY_ETHREAD;
			typedef struct _MY_KTHREAD_WIN10_1703_X64_ *PMY_KTHREAD, MY_KTHREAD;


			((PMY_KTHREAD)currentEThread)->Win32Thread = GetGuiThread(relProcEProcess);

			ULONG_PTR hActiveWindow = NtUserGetForegroundWindow();

			ULONG activeProcId = (ULONG)NtUserQueryWindow((HANDLE)hActiveWindow, 0);

			((PMY_KTHREAD)currentEThread)->Win32Thread = 0;

			KeUnstackDetachProcess(apcState);

			ExFreePool(apcState);

			if (activeProcId == g_RelatedProcessId)
			{
				return TRUE;
			}
			else
			{
				return FALSE;
			}
		}
		ExFreePool(relateName);
		return FALSE;
	}



	VOID ActiveWindowThread( PVOID StartContext )
	{
		while (TRUE)
		{
			LARGE_INTEGER delayTime = { 0 };

			delayTime = RtlConvertLongToLargeInteger(100 * -10000);

			NTSTATUS status = KeWaitForSingleObject(&g_ThreadTerminalEvent, Executive, KernelMode, FALSE, &delayTime);

			if (status == STATUS_SUCCESS)
			{
				PsTerminateSystemThread(STATUS_SUCCESS);
			}

			g_IsActive = IsRelatedWindowActive();


			PKTHREAD currentThread = KeGetCurrentThread();

			KeDelayExecutionThread(KernelMode, FALSE, &delayTime);

		}

	}


#ifdef __cplusplus
}
#endif