#ifdef __cplusplus
extern "C"{
#endif

#include <ntddk.h>
#include <ntddkbd.h>

#include "FuckKbdclass.h"
#include "ActiveWindow.h"
#include "Common.h"

	extern LIST_ENTRY g_KbdInfoList;
	extern BOOLEAN g_IsActive;

	PDRIVER_DISPATCH g_OriKbdclassReadDispatch = NULL;
	PIO_COMPLETION_ROUTINE g_OriKbdclassCompleteRoutineOfReadDispatch = NULL;
	ULONG g_IrpPendingNum = 0;
	ULONG g_ExtensionKeyWord = 0;		// ������¼ctrl��alt������ϼ��Ƿ񱻰���

	
	NTSTATUS InstallKbdclassIrpHook()
	{
		NTSTATUS status = STATUS_SUCCESS;

		PDRIVER_OBJECT kbdclassDriverObject = NULL;

		status = GetDriverObjectByName(L"\\Driver\\kbdclass", &kbdclassDriverObject);

		if (!NT_SUCCESS(status))
		{
			KdPrint(("KeyboardEncrypt ͨ����������ȡ��������ʧ��\n"));
			return status;
		}

		g_OriKbdclassReadDispatch = kbdclassDriverObject->MajorFunction[IRP_MJ_READ];

		KdPrint(("KeyboardEncrypt ԭ kbdclass �� Read ��ַΪ��0x%p\n", g_OriKbdclassReadDispatch));

		InterlockedExchangePointer((PVOID *)&kbdclassDriverObject->MajorFunction[IRP_MJ_READ], MyKbdclassReadDispatch);

		ObDereferenceObject(kbdclassDriverObject);

		return status;
	}


	NTSTATUS MyKbdclassReadDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
	{
		g_IrpPendingNum++;

		PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);

		irpSp->Control = SL_INVOKE_ON_CANCEL | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_SUCCESS;

		g_OriKbdclassCompleteRoutineOfReadDispatch = irpSp->CompletionRoutine;

		irpSp->CompletionRoutine = MyCompleteRoutineOfKbdclassReadDispatch;

		if (g_OriKbdclassReadDispatch)
		{
			return g_OriKbdclassReadDispatch(DeviceObject, Irp);
		}

		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;
	}

	NTSTATUS MyCompleteRoutineOfKbdclassReadDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
	{
		g_IrpPendingNum--;

		if (g_IsActive)
		{
			MarkAndModifyKeyboardInfo(Irp);
		}

		if (Irp->PendingReturned)
		{
			IoMarkIrpPending(Irp);
		}

		if (g_OriKbdclassCompleteRoutineOfReadDispatch)
		{
			return g_OriKbdclassCompleteRoutineOfReadDispatch(DeviceObject, Irp, Context);
		}


		return Irp->IoStatus.Status;
	}


	unsigned char asciiTbl[] = {
		/*
		0x00, 
		Esc, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, -, =, backspace, 
		tab,q, w, e, r, t, y, u, i, o, p, [, ], enter, 0x00, 
		a, s, d, f, g, h, j, k, l, ;, ', `, 0x00, \, 
		z, x, c, v, b, n, m, ,, ., /, 0x00, 
		Shift(Left), 0x00, space, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		7, 8, 9, -, 
		4, 5, 6, =, 
		1, 2, 3, 
		0, .,
		 */
		0x00, 0x1B, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x2D, 0x3D, 0x08, 0x09,	//normal
		0x71, 0x77, 0x65, 0x72, 0x74, 0x79, 0x75, 0x69, 0x6F, 0x70, 0x5B, 0x5D, 0x0D, 0x00, 0x61, 0x73,
		0x64, 0x66, 0x67, 0x68, 0x6A, 0x6B, 0x6C, 0x3B, 0x27, 0x60, 0x00, 0x5C, 0x7A, 0x78, 0x63, 0x76,
		0x62, 0x6E, 0x6D, 0x2C, 0x2E, 0x2F, 0x00, 0x2A, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x38, 0x39, 0x2D, 0x34, 0x35, 0x36, 0x2B, 0x31,
		0x32, 0x33, 0x30, 0x2E,
		0x00, 0x1B, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x2D, 0x3D, 0x08, 0x09,	//caps
		0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4F, 0x50, 0x5B, 0x5D, 0x0D, 0x00, 0x41, 0x53,
		0x44, 0x46, 0x47, 0x48, 0x4A, 0x4B, 0x4C, 0x3B, 0x27, 0x60, 0x00, 0x5C, 0x5A, 0x58, 0x43, 0x56,
		0x42, 0x4E, 0x4D, 0x2C, 0x2E, 0x2F, 0x00, 0x2A, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x38, 0x39, 0x2D, 0x34, 0x35, 0x36, 0x2B, 0x31,
		0x32, 0x33, 0x30, 0x2E,
		0x00, 0x1B, 0x21, 0x40, 0x23, 0x24, 0x25, 0x5E, 0x26, 0x2A, 0x28, 0x29, 0x5F, 0x2B, 0x08, 0x09,	//shift
		0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4F, 0x50, 0x7B, 0x7D, 0x0D, 0x00, 0x41, 0x53,
		0x44, 0x46, 0x47, 0x48, 0x4A, 0x4B, 0x4C, 0x3A, 0x22, 0x7E, 0x00, 0x7C, 0x5A, 0x58, 0x43, 0x56,
		0x42, 0x4E, 0x4D, 0x3C, 0x3E, 0x3F, 0x00, 0x2A, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x38, 0x39, 0x2D, 0x34, 0x35, 0x36, 0x2B, 0x31,
		0x32, 0x33, 0x30, 0x2E,
		0x00, 0x1B, 0x21, 0x40, 0x23, 0x24, 0x25, 0x5E, 0x26, 0x2A, 0x28, 0x29, 0x5F, 0x2B, 0x08, 0x09,	//caps + shift
		0x71, 0x77, 0x65, 0x72, 0x74, 0x79, 0x75, 0x69, 0x6F, 0x70, 0x7B, 0x7D, 0x0D, 0x00, 0x61, 0x73,
		0x64, 0x66, 0x67, 0x68, 0x6A, 0x6B, 0x6C, 0x3A, 0x22, 0x7E, 0x00, 0x7C, 0x7A, 0x78, 0x63, 0x76,
		0x62, 0x6E, 0x6D, 0x3C, 0x3E, 0x3F, 0x00, 0x2A, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x38, 0x39, 0x2D, 0x34, 0x35, 0x36, 0x2B, 0x31,
		0x32, 0x33, 0x30, 0x2E
	};

	// flags for keyboard status
#define	S_SHIFT				1
#define	S_CAPS				2
#define	S_NUM				4
	static int kb_status = S_NUM;
	int		asciiTblOffset = 0;
	UCHAR __stdcall GetMakeCodeAscii(UCHAR sch)
	{
		UCHAR	ch = 0;


		// ������һ��ʱ������ mark ��
		// �ſ���ʱ������ break ��
		// break �� mark ��� bit7 �� 1 ������Ҳ���ǣ� break = mark + 0x80
		// http://www.voidcn.com/article/p-syxmbgac-sm.html
		// https://wenku.baidu.com/view/bee9427c168884868762d6f3.html



		if (kb_status & S_SHIFT && kb_status & S_CAPS)
		{
			asciiTblOffset = 84 * 3;
		}
		else
		{
			if (kb_status & S_SHIFT)
			{
				asciiTblOffset = 84 * 2;
			}
			else if (kb_status & S_CAPS)
			{
				asciiTblOffset = 84;
			}
			else
			{
				asciiTblOffset = 0;
			}
		}

		if ((sch & 0x80) == 0)	//make
		{
			if ((sch < 0x47) || // ������
				((sch >= 0x47 && sch < 0x54) && (kb_status & S_NUM))) // Num Lock С���� ���ּ���
			{
				ch = asciiTbl[asciiTblOffset + sch];
			}
		}


		return ch;
	}


	VOID MarkAndModifyKeyboardInfo(PIRP Irp)
	{
		if (NT_SUCCESS(Irp->IoStatus.Status))
		{
			PVOID kbdBuf = Irp->AssociatedIrp.SystemBuffer;

			PKEYBOARD_INPUT_DATA kbdData = (PKEYBOARD_INPUT_DATA)kbdBuf;

			ULONG kbdDataLen = Irp->IoStatus.Information;

			ULONG keywordNum = kbdDataLen / sizeof(PKEYBOARD_INPUT_DATA);


			for (int i = 0; i < keywordNum; i++)
			{
				// �󲿷ּ��̰���״̬��key_make ̧��Ϊbreak


				// �еļ��̰����� key_e0 | key_make
				// ̧���� key_e0 | key_break ���ұߵ�ctrl �� alt

				if (kbdData->Flags & KEY_BREAK)
				{
					KdPrint(("KeyboardEncrypt ScanCode: %x\n", kbdData->MakeCode));

					switch (kbdData->MakeCode)
					{
					case 0x3A:
						kb_status ^= S_CAPS;
						break;

					case 0x2A:
					case 0x36:
						kb_status &= ~S_SHIFT;
						break;

					case 0x45:
						kb_status ^= S_NUM;
					}


					if (kbdData->MakeCode == 0x1d || kbdData->MakeCode == 0x38)
					{
						// ctrl alt

						g_ExtensionKeyWord = 0;
					}

					// ��ݼ����ֵı�����ʱ������¼����
					if (!g_ExtensionKeyWord)
					{
						// ֻ��¼��ĸ������
						if ((kbdData->MakeCode >= 0x10 && kbdData->MakeCode <= 0x19) ||				// q -- p
							(kbdData->MakeCode >= 0x1E && kbdData->MakeCode <= 0x26) ||				// a -- l
							(kbdData->MakeCode >= 0x2C && kbdData->MakeCode <= 0x32) ||				// z -- m
							(kbdData->MakeCode >= 0x2 && kbdData->MakeCode <= 0xB) )				// 1 -- 0
						{
							// ��¼������
							PKEYBOARDINFO keyboardInfo = (PKEYBOARDINFO)ExAllocatePoolWithTag(NonPagedPool, sizeof(KEYBOARDINFO), KBDTAG);

							UCHAR asciiCode = GetMakeCodeAscii((UCHAR)kbdData->MakeCode);

							keyboardInfo->makecode = kbdData->MakeCode;
							keyboardInfo->encryptoValue = asciiCode ^ 0xFFFF;

							InsertHeadList(&g_KbdInfoList, &keyboardInfo->listentry);
						}


					}

				}
				else
				{
					if ((UCHAR)kbdData->MakeCode == 0x2a || (UCHAR)kbdData->MakeCode == 0x36)
					{
						kb_status |= S_SHIFT;
					}

					if (kbdData->MakeCode == 0x1d || kbdData->MakeCode == 0x38)
					{
						// ctrl alt

						g_ExtensionKeyWord = 1;
					}
				}


				// �޸�
				kbdData->MakeCode = 0x31; // 'n'
			}

		}
	}

#ifdef __cplusplus
}
#endif
