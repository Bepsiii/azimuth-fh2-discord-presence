using System;
using System.Runtime.InteropServices;
public static class LoaderChecks {
 [DllImport("kernel32",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr LoadLibraryEx(string file,IntPtr reserved,uint flags);
 [DllImport("kernel32",CharSet=CharSet.Unicode)] static extern IntPtr GetModuleHandle(string name);
 [DllImport("kernel32",CharSet=CharSet.Ansi)] static extern IntPtr GetProcAddress(IntPtr module,string name);
 [DllImport("kernel32")] static extern uint GetCurrentThreadId();
 [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr TextFunction();
 static void Require(bool ok,string message){if(!ok)throw new Exception(message);}
 static int Dir(IntPtr m,int index){int pe=Marshal.ReadInt32(m,60);return Marshal.ReadInt32(m,pe+24+112+index*8);}
 public static void Check(string filename,bool extras){
  uint thread=GetCurrentThreadId();IntPtr m=LoadLibraryEx(filename,IntPtr.Zero,0x100|0x800);
  Require(m!=IntPtr.Zero,"Loader failed: "+Marshal.GetLastWin32Error());
  IntPtr index=GetProcAddress(m,"azimuth_tls_index"),attached=GetProcAddress(m,"azimuth_tls_attached");
  Require(index!=IntPtr.Zero&&attached!=IntPtr.Zero,"Missing native TLS data exports");
  Require(Marshal.ReadInt32(index)!=-1,"TLS index was not allocated");
  Require(unchecked((uint)Marshal.ReadInt32(attached))==thread,"TLS attach thread changed");
  int tls=Dir(m,9);Require(tls!=0,"Missing TLS directory");
  Require(Marshal.ReadInt64(m,tls+8)-Marshal.ReadInt64(m,tls)==336,"Wrong TLS template size");
  Require(Marshal.ReadInt64(m,tls+16)==index.ToInt64(),"TLS directory index does not match export");
  Require(Marshal.ReadInt32(m,tls+32)==0&&Marshal.ReadInt32(m,tls+36)==0x500000,"TLS fill/alignment mismatch");
  Require(Marshal.ReadInt64(new IntPtr(Marshal.ReadInt64(m,tls+24)))==0,"Unexpected TLS callback");
  IntPtr presence=GetModuleHandle("FH2DiscordPresence.dll");Require(presence!=IntPtr.Zero,"Independent presence DLL did not load");
  var version=(TextFunction)Marshal.GetDelegateForFunctionPointer(GetProcAddress(presence,"FH2DiscordPresenceVersion"),typeof(TextFunction));
  Require(Marshal.PtrToStringAnsi(version())=="1.0.0","Wrong presence version");
  Require(Dir(presence,9)==0,"Presence DLL still carries the TLS adapter");
  int imports=Dir(presence,1);for(int p=imports;Marshal.ReadInt32(presence,p+12)!=0;p+=20){string name=Marshal.PtrToStringAnsi(IntPtr.Add(presence,Marshal.ReadInt32(presence,p+12)));Require(String.Equals(name,"KERNEL32.dll",StringComparison.OrdinalIgnoreCase),"Unexpected presence dependency: "+name);}
  Require((GetModuleHandle("FH2_ExtraOptions.dll")!=IntPtr.Zero)==extras,"Extra Options load selection mismatch");
  Console.WriteLine("PASS: native import loading, 336-byte TLS template, TLS index/thread exports, independent presence 1.0.0, "+(extras?"separate Extra Options":"no Extra Options dependency"));
 }
}
