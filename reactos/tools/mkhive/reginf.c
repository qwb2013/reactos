/*
 *  ReactOS kernel
 *  Copyright (C) 2003 ReactOS Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/* $Id$
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS hive maker
 * FILE:            tools/mkhive/reginf.h
 * PURPOSE:         Inf file import code
 * PROGRAMMER:      Eric Kohl
 */

/* INCLUDES *****************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "mkhive.h"
#include "registry.h"
#include <infhost.h>



#define FLG_ADDREG_BINVALUETYPE           0x00000001
#define FLG_ADDREG_NOCLOBBER              0x00000002
#define FLG_ADDREG_DELVAL                 0x00000004
#define FLG_ADDREG_APPEND                 0x00000008
#define FLG_ADDREG_KEYONLY                0x00000010
#define FLG_ADDREG_OVERWRITEONLY          0x00000020
#define FLG_ADDREG_TYPE_SZ                0x00000000
#define FLG_ADDREG_TYPE_MULTI_SZ          0x00010000
#define FLG_ADDREG_TYPE_EXPAND_SZ         0x00020000
#define FLG_ADDREG_TYPE_BINARY           (0x00000000 | FLG_ADDREG_BINVALUETYPE)
#define FLG_ADDREG_TYPE_DWORD            (0x00010000 | FLG_ADDREG_BINVALUETYPE)
#define FLG_ADDREG_TYPE_NONE             (0x00020000 | FLG_ADDREG_BINVALUETYPE)
#define FLG_ADDREG_TYPE_MASK             (0xFFFF0000 | FLG_ADDREG_BINVALUETYPE)


/* FUNCTIONS ****************************************************************/

static BOOL
GetRootKey (PCHAR Name)
{
  if (!strcasecmp (Name, "HKCR"))
    {
      strcpy (Name, "\\Registry\\Machine\\SOFTWARE\\Classes\\");
      return TRUE;
    }

  if (!strcasecmp (Name, "HKCU"))
    {
      strcpy (Name, "\\Registry\\User\\.DEFAULT\\");
      return TRUE;
    }

  if (!strcasecmp (Name, "HKLM"))
    {
      strcpy (Name, "\\Registry\\Machine\\");
      return TRUE;
    }

  if (!strcasecmp (Name, "HKU"))
    {
      strcpy (Name, "\\Registry\\User\\");
      return TRUE;
    }

#if 0
  if (!strcasecmp (Name, "HKR"))
    return FALSE;
#endif

  return FALSE;
}


/***********************************************************************
 * AppendMultiSzValue
 *
 * Append a multisz string to a multisz registry value.
 */
static VOID
AppendMultiSzValue (HKEY KeyHandle,
		    PCHAR ValueName,
		    PCHAR Strings,
		    ULONG StringSize)
{
  ULONG Size;
  ULONG Type;
  ULONG Total;
  PCHAR Buffer;
  PCHAR p;
  int len;
  LONG Error;

  Error = RegQueryValue (KeyHandle,
			 ValueName,
			 &Type,
			 NULL,
			 &Size);
  if ((Error != ERROR_SUCCESS) ||
      (Type != REG_MULTI_SZ))
    return;

  Buffer = (char *)malloc (Size + StringSize);
  if (Buffer == NULL)
     return;

  Error = RegQueryValue (KeyHandle,
			 ValueName,
			 NULL,
			 (PCHAR)Buffer,
			 &Size);
  if (Error != ERROR_SUCCESS)
     goto done;

  /* compare each string against all the existing ones */
  Total = Size;
  while (*Strings != 0)
    {
      len = strlen (Strings) + 1;

      for (p = Buffer; *p != 0; p += strlen (p) + 1)
        if (!strcasecmp (p, Strings))
          break;

      if (*p == 0)  /* not found, need to append it */
        {
          memcpy (p, Strings, len);
          p[len] = 0;
          Total += len;
        }
      Strings += len;
    }

  if (Total != Size)
    {
      DPRINT ("setting value %s to %s\n", ValueName, Buffer);
      RegSetValue (KeyHandle,
		   ValueName,
		   REG_MULTI_SZ,
		   (PCHAR)Buffer,
		   Total);
    }

done:
  free (Buffer);
}


/***********************************************************************
 *            do_reg_operation
 *
 * Perform an add/delete registry operation depending on the flags.
 */
static BOOL
do_reg_operation(HKEY KeyHandle,
		 PCHAR ValueName,
		 PINFCONTEXT Context,
		 ULONG Flags)
{
  CHAR EmptyStr = (CHAR)0;
  ULONG Type;
  ULONG Size;
  LONG Error;

  if (Flags & FLG_ADDREG_DELVAL)  /* deletion */
    {
      if (ValueName)
	{
	  RegDeleteValue (KeyHandle,
			  ValueName);
	}
      else
	{
	  RegDeleteKey (KeyHandle,
			NULL);
	}

      return TRUE;
    }

  if (Flags & FLG_ADDREG_KEYONLY)
    return TRUE;

  if (Flags & (FLG_ADDREG_NOCLOBBER | FLG_ADDREG_OVERWRITEONLY))
    {
      Error = RegQueryValue (KeyHandle,
			     ValueName,
			     NULL,
			     NULL,
			     NULL);
      if ((Error == ERROR_SUCCESS) &&
	  (Flags & FLG_ADDREG_NOCLOBBER))
	return TRUE;

      if ((Error != ERROR_SUCCESS) &&
	  (Flags & FLG_ADDREG_OVERWRITEONLY))
	return TRUE;
    }

  switch (Flags & FLG_ADDREG_TYPE_MASK)
    {
      case FLG_ADDREG_TYPE_SZ:
	Type = REG_SZ;
	break;

      case FLG_ADDREG_TYPE_MULTI_SZ:
	Type = REG_MULTI_SZ;
	break;

      case FLG_ADDREG_TYPE_EXPAND_SZ:
	Type = REG_EXPAND_SZ;
	break;

      case FLG_ADDREG_TYPE_BINARY:
	Type = REG_BINARY;
	break;

      case FLG_ADDREG_TYPE_DWORD:
	Type = REG_DWORD;
	break;

      case FLG_ADDREG_TYPE_NONE:
	Type = REG_NONE;
	break;

      default:
	Type = Flags >> 16;
	break;
    }

  if (!(Flags & FLG_ADDREG_BINVALUETYPE) ||
      (Type == REG_DWORD && InfHostGetFieldCount (Context) == 5))
    {
      PCHAR Str = NULL;

      if (Type == REG_MULTI_SZ)
	{
	  if (InfHostGetMultiSzField (Context, 5, NULL, 0, &Size) != 0)
	    Size = 0;

	  if (Size)
	    {
	      Str = (char *)malloc (Size);
	      if (Str == NULL)
		return FALSE;

	      InfHostGetMultiSzField (Context, 5, Str, Size, NULL);
	    }

	  if (Flags & FLG_ADDREG_APPEND)
	    {
	      if (Str == NULL)
		return TRUE;

	      AppendMultiSzValue (KeyHandle,
				  ValueName,
				  Str,
				  Size);

	      free (Str);
	      return TRUE;
	    }
	  /* else fall through to normal string handling */
	}
      else
	{
	  if (InfHostGetStringField (Context, 5, NULL, 0, &Size) != 0)
	    Size = 0;

	  if (Size)
	    {
	      Str = (char *)malloc (Size);
	      if (Str == NULL)
		return FALSE;

	      InfHostGetStringField (Context, 5, Str, Size, NULL);
	    }
	}

      if (Type == REG_DWORD)
	{
	  ULONG dw = Str ? strtoul (Str, NULL, 0) : 0;

	  DPRINT("setting dword %s to %lx\n", ValueName, dw);

	  RegSetValue (KeyHandle,
		       ValueName,
		       Type,
		       (char *)&dw,
		       sizeof(ULONG));
	}
      else
	{
	  DPRINT("setting value %s to %s\n", ValueName, Str);

	  if (Str)
	    {
	      RegSetValue (KeyHandle,
			   ValueName,
			   Type,
			   (char *)Str,
			   Size);
	    }
	  else
	    {
	      RegSetValue (KeyHandle,
			   ValueName,
			   Type,
			   (char *)&EmptyStr,
			   sizeof(CHAR));
	    }
	}
      free (Str);
    }
  else  /* get the binary data */
    {
      PCHAR Data = NULL;

      if (InfHostGetBinaryField (Context, 5, NULL, 0, &Size) != 0)
	Size = 0;

      if (Size)
	{
	    Data = (char *)malloc (Size);
	  if (Data == NULL)
	    return FALSE;

	  DPRINT("setting binary data %s len %lu\n", ValueName, Size);
	  InfHostGetBinaryField (Context, 5, (unsigned char *)Data, Size, NULL);
	}

      RegSetValue (KeyHandle,
		   ValueName,
		   Type,
		   (char *)Data,
		   Size);

      free (Data);
    }

  return TRUE;
}


/***********************************************************************
 *            registry_callback
 *
 * Called once for each AddReg and DelReg entry in a given section.
 */
static BOOL
registry_callback (HINF hInf, PCHAR Section, BOOL Delete)
{
  CHAR Buffer[MAX_INF_STRING_LENGTH];
  PCHAR ValuePtr;
  ULONG Flags;
  ULONG Length;

  PINFCONTEXT Context = NULL;
  HKEY KeyHandle;
  BOOL Ok;


  Ok = InfHostFindFirstLine (hInf, Section, NULL, &Context) == 0;
  if (!Ok)
    return TRUE; /* Don't fail if the section isn't present */

  for (;Ok; Ok = (InfHostFindNextLine (Context, Context) == 0))
    {
      /* get root */
      if (InfHostGetStringField (Context, 1, Buffer, MAX_INF_STRING_LENGTH, NULL) != 0)
	continue;
      if (!GetRootKey (Buffer))
	continue;

      /* get key */
      Length = strlen (Buffer);
      if (InfHostGetStringField (Context, 2, Buffer + Length, MAX_INF_STRING_LENGTH - Length, NULL) != 0)
	*Buffer = 0;

      DPRINT("KeyName: <%s>\n", Buffer);

      if (Delete)
        {
          Flags = FLG_ADDREG_DELVAL;
        }
      else
        {
          /* get flags */
          if (InfHostGetIntField (Context, 4, (PULONG)&Flags) != 0)
            Flags = 0;
        }

      DPRINT("Flags: %lx\n", Flags);

      if (Delete || (Flags & FLG_ADDREG_OVERWRITEONLY))
	{
	  if (RegOpenKey (NULL, Buffer, &KeyHandle) != ERROR_SUCCESS)
	    {
	      DPRINT("RegOpenKey(%s) failed\n", Buffer);
	      continue;  /* ignore if it doesn't exist */
	    }
	}
      else
	{
	  if (RegCreateKey (NULL, Buffer, &KeyHandle) != ERROR_SUCCESS)
	    {
	      DPRINT("RegCreateKey(%s) failed\n", Buffer);
	      continue;
	    }
	}

      /* get value name */
      if (InfHostGetStringField (Context, 3, Buffer, MAX_INF_STRING_LENGTH, NULL) == 0)
	{
	  ValuePtr = Buffer;
	}
      else
	{
	  ValuePtr = NULL;
	}

      /* and now do it */
      if (!do_reg_operation (KeyHandle, ValuePtr, Context, Flags))
	{
	  return FALSE;
	}
    }

  InfHostFreeContext(Context);

  return TRUE;
}


BOOL
ImportRegistryFile(PCHAR FileName,
		   PCHAR Section,
		   BOOL Delete)
{
  HINF hInf;
  ULONG ErrorLine;

  /* Load inf file from install media. */
  if (InfHostOpenFile(&hInf, FileName, &ErrorLine) != 0)
    {
      DPRINT1 ("InfHostOpenFile() failed\n");
      return FALSE;
    }

  if (!registry_callback (hInf, "DelReg", TRUE))
    {
      DPRINT1 ("registry_callback() failed\n");
    }

  if (!registry_callback (hInf, "AddReg", FALSE))
    {
      DPRINT1 ("registry_callback() failed\n");
    }

  InfHostCloseFile (hInf);

  return TRUE;
}

/* EOF */
