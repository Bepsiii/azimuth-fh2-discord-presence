using System;
using System.Text;
using System.Runtime.InteropServices;
using System.IO;
using System.IO.Pipes;
using System.Threading;
using System.Text.RegularExpressions;
using System.Diagnostics;
[StructLayout(LayoutKind.Sequential)] public struct NativePresenceSample {
 public int loaded,paused,car_id,activity,has_position; public float x,z;
}
public static class NativePresenceChecks {
 [DllImport(@"DLL_PATH",CallingConvention=CallingConvention.Cdecl)] static extern void Fh2PresenceTestBind(IntPtr image);
 [DllImport(@"DLL_PATH",CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] static extern int Fh2TestReadOptions(string directory,IntPtr options);
 [DllImport(@"DLL_PATH",CallingConvention=CallingConvention.Cdecl)] static extern NativePresenceSample Fh2PresenceTestSample();
 [DllImport(@"DLL_PATH",CallingConvention=CallingConvention.Cdecl)] static extern int Fh2PresenceTestFormat(NativePresenceSample s,[Out] byte[] output,uint capacity);
 [DllImport(@"DLL_PATH",CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Ansi)] static extern int Fh2PresenceTestField(string json,string key,[Out] byte[] output,uint capacity);
 [DllImport(@"DLL_PATH",CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] static extern int Fh2PresenceTestSession(string name,[MarshalAs(UnmanagedType.LPStr)] string activity);
 [DllImport(@"DLL_PATH",CallingConvention=CallingConvention.Cdecl)] static extern int Fh2PresenceTestId([MarshalAs(UnmanagedType.LPStr)] string id);
 [DllImport(@"DLL_PATH",CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] static extern int Fh2PresenceTestWorker(string directory,string name);
 [DllImport(@"DLL_PATH",CallingConvention=CallingConvention.Cdecl)] static extern void Fh2PresenceTestStop();
 [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr VirtualAlloc(IntPtr address,UIntPtr size,uint type,uint protect);
 [DllImport("kernel32.dll")] static extern bool VirtualFree(IntPtr address,UIntPtr size,uint type);
 static long B=0x140000000;
 static void Bytes(long a,byte[] b){Marshal.Copy(b,0,new IntPtr(a),b.Length);}
 static void Pointer(long a,long p){Bytes(a,BitConverter.GetBytes(p));}
 static void Int(long a,int n){Bytes(a,BitConverter.GetBytes(n));}
 static string Text(byte[] b){int n=Array.IndexOf(b,(byte)0);return Encoding.UTF8.GetString(b,0,n<0?b.Length:n);}
 static void Require(bool result,string message){if(!result)throw new Exception(message);}
 public static string CheckJson(){
  byte[] output=new byte[512];
  string[] invalid={"", "[]", "{", "{\"evt\":\"READY\"} trailing", "{\"evt\":\"READY\",}",
   "{\"evt\":\"READY\",\"evt\":\"ERROR\"}","{\"evt\":\"READY\\u0000bad\"}",
   "{\"evt\":\"\\q\"}","{\"evt\":\"\\uD800\"}","{\"evt\":123}",
   "{\"evt\":\"READY\",\"x\":01}","{\"x\":1e}","{\"x\":.1}","{\"x\":[1,]}",
   "{\"evt\":\"unterminated}","{\"evt\":\"raw\nnewline\"}",
   "{\"x\":"+new string('[',40)+"0"+new string(']',40)+"}"};
  foreach(string json in invalid)Require(Fh2PresenceTestField(json,"evt",output,512)==-1,"Accepted invalid JSON: "+json);
  Require(Fh2PresenceTestField("{\"data\":{\"evt\":\"ERROR\"},\"evt\":\"READY\"}","evt",output,512)==1&&Text(output)=="READY","Nested field shadowed root");
  Require(Fh2PresenceTestField("{\"data\":{\"evt\":\"READY\"}}","evt",output,512)==0,"Nested field accepted as root");
  Require(Fh2PresenceTestField("{\"evt\":null}","evt",output,512)==0,"Null event rejected");
  Require(Fh2PresenceTestField("{\"e\\u0076t\":\"R\\u0045ADY\",\"x\":[true,false,null,-1.25e+3]}","evt",output,512)==1&&Text(output)=="READY","Escapes/literals failed");
  Require(Fh2PresenceTestField("{\"evt\":\"\\u00e9\\u20ac\\ud83d\\ude97\"}","evt",output,512)==1&&Text(output)=="\u00e9\u20ac\ud83d\ude97","Unicode decoding failed");
  Require(Fh2PresenceTestField("{\"evt\":\"READY\"}","evt",output,5)==-1,"Truncated field accepted");
  Require(Fh2PresenceTestField("{\"evt\":\"READY\"}","evt",output,0)==-1,"Zero output capacity accepted");
  foreach(string id in new[]{"0","","-1","123a","18446744073709551616",new string('9',50)})Require(Fh2PresenceTestId(id)==0,"Invalid application ID accepted");
  Require(Fh2PresenceTestId("1559239981548052682")==1&&Fh2PresenceTestId("18446744073709551615")==1,"Valid application ID rejected");
  return "PASS: strict JSON structure, nesting, duplicates, Unicode, bounds and application IDs";
 }
 public static string CheckConfiguration(string root){
  string[] contents={"Enabled=0","ApplicationId=invalid","ApplicationId=18446744073709551616","LargeImage="+new string('x',300),"ShowPaused=maybe"};
  string[] messages={"disabled by configuration","invalid ApplicationId","invalid ApplicationId","too long or invalid","must be 0 or 1"};
  for(int i=0;i<contents.Length;i++){
   string directory=Path.Combine(root,"config-"+i);Directory.CreateDirectory(directory);
   File.WriteAllText(Path.Combine(directory,"FH2_DiscordPresence.ini"),"[Discord]\r\n"+contents[i]+"\r\n",new UTF8Encoding(false));
   var worker=new Thread(delegate(){Fh2PresenceTestWorker(directory,"\\\\.\\pipe\\fh2-never-connect-"+Guid.NewGuid().ToString("N"));});worker.IsBackground=true;worker.Start();
   bool stopped=worker.Join(1500);if(!stopped){Fh2PresenceTestStop();worker.Join(1500);}
   Require(stopped&&File.ReadAllText(Path.Combine(directory,"FH2_DiscordPresence.log")).Contains(messages[i]),"Invalid/disabled configuration did not stop: "+contents[i]);
  }
  return "PASS: disabled presence, invalid/overflowing application IDs, oversized settings and invalid booleans";
 }
 public static string CheckDefaults(string directory){
  IntPtr options=Marshal.AllocHGlobal(256);
  try {Require(Fh2TestReadOptions(directory,options)==1&&Marshal.ReadInt32(options)==0,"Missing Extra Options INI enabled gameplay patches");}
  finally {Marshal.FreeHGlobal(options);}
  return "PASS: single-DLL install without Extra Options INI keeps gameplay patches disabled";
 }
 public static string CheckMemory(string directory){
  IntPtr allocation=VirtualAlloc(new IntPtr(B),new UIntPtr(0x4000000),0x3000,4);
  Require(allocation!=IntPtr.Zero,"Fixture allocation failed");
  try{
   long game=B+0x3b00000,sim=game+0x1000,scene=sim+0x1000,car=scene+0x1000,traffic=car+0x7000,vec=traffic+0x7000;
   long manager=vec+0x1000,player=manager+0x1000,players=player+0x1000,playerNode=players+0x1000;
   Pointer(B+0x34aba60,game);Pointer(game+0x48,sim);Pointer(sim+0x98,scene);
   Pointer(scene+0x458,vec);Pointer(scene+0x460,vec+16);Pointer(vec,car);Pointer(vec+8,traffic);
   Pointer(car,B+0x2c5cb80);Int(car+0x118,1218);Pointer(car+0x4d00,player);
   Pointer(traffic,B+0x2c5cb80);Int(traffic+0x118,379);Int(traffic+0x4d08,2);
   Pointer(B+0x34b3e18,manager);Pointer(manager,B+0x2c62a40);Bytes(manager+0x34,new byte[]{1});Pointer(manager+0x40,1);
   Pointer(manager+0x38,players);Pointer(players,playerNode);Pointer(playerNode+0x10,player);
   Bytes(car+0x4f50,BitConverter.GetBytes(9587.42285f));Bytes(car+0x4f54,BitConverter.GetBytes(4.37285f));Bytes(car+0x4f58,BitConverter.GetBytes(-7860.873f));
   long hud=playerNode+0x1000,services=hud+0x1000,head=services+0x1000,node=head+0x1000,holder=node+0x1000,wrapper=holder+0x1000,module=wrapper+0x1000;
   Pointer(B+0x34c0f40,hud);Pointer(hud+8,services);Pointer(services+0xf8,head);Pointer(head,node);Pointer(node,head);
   Pointer(node+0x20,1);Pointer(node+0x18,holder);Pointer(holder,wrapper);Pointer(wrapper+0x10,module);Pointer(module,B+0x2cbb130);Int(module+0x70,2);
   Fh2PresenceTestBind(allocation);NativePresenceSample s=Fh2PresenceTestSample();
   Require(s.loaded==1&&s.car_id==1218&&s.activity==2&&s.has_position==1&&s.paused==0,"Native sample mismatch");
   byte[] output=new byte[4096];Require(Fh2PresenceTestFormat(s,output,4096)==1,"Native format failed");
   string json=Text(output);Require(json.Contains("2011 BMW Z4 sDrive35is")&&json.Contains("Free roam | Near Castelletto"),"Capture replay mismatch");
   NativePresenceSample catalog=s;catalog.car_id=251;Require(Fh2PresenceTestFormat(catalog,output,4096)==1&&Text(output).Contains("Coup\u00e9"),"UTF-8 car name changed");
   catalog.car_id=265;Require(Fh2PresenceTestFormat(catalog,output,4096)==1&&Text(output).Contains("Porsche"),"DLC manufacturer name missing");
   catalog.car_id=999999;Require(Fh2PresenceTestFormat(catalog,output,4096)==1&&Text(output).Contains("Car #999999"),"Unknown car fallback failed");
   Bytes(sim+0x28,new byte[]{1});s=Fh2PresenceTestSample();Require(s.paused==1,"Native pause transition failed");
   Require(Fh2PresenceTestFormat(s,output,4096)==1&&Text(output).Contains("Paused | Near Castelletto"),"Pause formatting failed");
   Bytes(sim+0x28,new byte[]{0});Int(module+0x70,6);s=Fh2PresenceTestSample();
   Require(Fh2PresenceTestFormat(s,output,4096)==1&&Text(output).Contains("Cross-country racing"),"Race mapping failed");
   Int(module+0x70,23);s=Fh2PresenceTestSample();Require(Fh2PresenceTestFormat(s,output,4096)==1&&Text(output).Contains("Storm Island"),"Island mapping failed");
   Require(!Text(output).Contains("| Storm Island"),"Repeated Island label");
   Pointer(B+0x34b3e18,0);Require(Fh2PresenceTestSample().loaded==0,"Missing player manager fell back to a guessed car");Pointer(B+0x34b3e18,manager);
   Pointer(players,players);Require(Fh2PresenceTestSample().loaded==0,"Empty player list accepted");Pointer(players,playerNode);
   long secondNode=module+0x1000;Pointer(playerNode,secondNode);Pointer(secondNode+0x10,player);Pointer(manager+0x40,2);Bytes(manager+0x34,new byte[]{0});
   Require(Fh2PresenceTestSample().loaded==1,"Second-list-entry player selection failed");Bytes(manager+0x34,new byte[]{1});Pointer(manager+0x40,1);
   Pointer(scene+0x460,vec+513);Require(Fh2PresenceTestSample().loaded==0,"Oversized vector accepted");Pointer(scene+0x460,vec+16);
   Pointer(scene+0x458,1);Pointer(scene+0x460,9);Require(Fh2PresenceTestSample().loaded==0,"Inaccessible vector accepted");Pointer(scene+0x458,vec);Pointer(scene+0x460,vec+16);
   Bytes(car+0x4f50,BitConverter.GetBytes(float.NaN));Require(Fh2PresenceTestSample().has_position==0,"NaN position accepted");Bytes(car+0x4f50,BitConverter.GetBytes(9587.42285f));
   Int(car+0x118,0);Require(Fh2PresenceTestSample().loaded==0,"Invalid car ID accepted");Int(car+0x118,1218);
   Int(car+0x118,1215);Require(Fh2PresenceTestSample().loaded==0,"Internal NULL CAR accepted");Int(car+0x118,1218);
   Pointer(car+0x4d00,traffic);Require(Fh2PresenceTestSample().loaded==0,"Wrong player's car accepted");Pointer(car+0x4d00,player);
   Int(traffic+0x4d08,0);Pointer(traffic+0x4d00,player);Require(Fh2PresenceTestSample().loaded==0,"Ambiguous cars accepted");
   Int(traffic+0x4d08,2);Int(module+0x70,2);
   CheckWorker(directory,sim,car,manager);
   Require(Fh2PresenceTestField("{ \"evt\" : \"READY\" }","evt",output,4096)==1&&Text(output)=="READY","JSON whitespace field parsing failed");
   return json;
  }finally{VirtualFree(allocation,UIntPtr.Zero,0x8000);}
 }
 static byte[] Exact(Stream stream,int count){byte[] b=new byte[count];int at=0;while(at<count){var task=stream.ReadAsync(b,at,count-at);if(!task.Wait(5000))throw new TimeoutException("Mock read timed out");int n=task.Result;if(n==0)throw new IOException("Closed");at+=n;}return b;}
 static string Receive(Stream stream,int opcode){byte[] header=Exact(stream,8);Require(BitConverter.ToInt32(header,0)==opcode,"Native IPC opcode mismatch");int size=BitConverter.ToInt32(header,4);Require(size>=0&&size<=65536,"Native frame length");return Encoding.UTF8.GetString(Exact(stream,size));}
 static byte[] Frame(int opcode,string body){byte[] b=Encoding.UTF8.GetBytes(body),outBytes=new byte[8+b.Length];Array.Copy(BitConverter.GetBytes(opcode),outBytes,4);Array.Copy(BitConverter.GetBytes(b.Length),0,outBytes,4,4);Array.Copy(b,0,outBytes,8,b.Length);return outBytes;}
 static void Send(Stream stream,int opcode,string body){byte[] frame=Frame(opcode,body);stream.Write(frame,0,frame.Length);stream.Flush();}
 static void Ack(Stream stream,string request){var match=Regex.Match(request,"\"nonce\":\"([0-9]+)\"");Require(match.Success,"Missing request nonce");Send(stream,1,"{\"cmd\":\"SET_ACTIVITY\",\"evt\":null,\"nonce\":\""+match.Groups[1].Value+"\",\"data\":{}}");}
 static void Ready(Stream stream){Receive(stream,0);Send(stream,1,"{\"cmd\":\"DISPATCH\",\"evt\":\"READY\",\"data\":{}}");}
 static void CheckWorker(string directory,long sim,long car,long manager){
  string name="fh2-worker-test-"+Guid.NewGuid().ToString("N");Exception failure=null;
  using(var server=new NamedPipeServerStream(name,PipeDirection.InOut,1,PipeTransmissionMode.Byte,PipeOptions.Asynchronous,4096,4096)){
   var worker=new Thread(delegate(){try{Fh2PresenceTestWorker(directory,"\\\\.\\pipe\\"+name);}catch(Exception e){failure=e;}});worker.IsBackground=true;worker.Start();
   try{
    Require(server.WaitForConnectionAsync().Wait(5000),"Worker did not connect");Ready(server);
    string first=Receive(server,1);Require(first.Contains("2011 BMW Z4 sDrive35is")&&first.Contains("Near Castelletto"),"Worker initial activity mismatch");Ack(server,first);
    var timer=Stopwatch.StartNew();Bytes(sim+0x28,new byte[]{1});
    string paused=Receive(server,1);Require(paused.Contains("Paused")&&timer.ElapsedMilliseconds>=180,"Worker pause/rate limit failed");Ack(server,paused);
    var start=Regex.Match(first,"\"start\":([0-9]+)").Value;
    // Drop an acknowledged connection and expect a complete fresh handshake.
    server.Disconnect();Require(server.WaitForConnectionAsync().Wait(5000),"Worker did not reconnect");Ready(server);
    string resumed=Receive(server,1);Require(resumed.Contains(start),"Reconnect reset session timer");
    // Withhold an acknowledgement; the worker must abandon this connection.
    Thread.Sleep(1300);byte[] closed=new byte[1];var closeRead=server.ReadAsync(closed,0,1);Require(closeRead.Wait(5000)&&closeRead.Result==0,"Missing acknowledgement did not disconnect");
    server.Disconnect();Require(server.WaitForConnectionAsync().Wait(5000),"Worker did not retry after missing acknowledgement");Ready(server);
    resumed=Receive(server,1);Ack(server,resumed);
    Pointer(B+0x34b3e18,0);string clear=Receive(server,1);Require(clear.Contains("\"activity\":null"),"Worker did not clear unavailable gameplay");Ack(server,clear);
    Pointer(B+0x34b3e18,manager);Bytes(sim+0x28,new byte[]{0});Int(car+0x118,379);
    string changed=Receive(server,1);Require(!changed.Contains("Z4 sDrive35is")&&!changed.Contains("\"activity\":null"),"Worker failed to publish changed car");Ack(server,changed);
   }finally{Fh2PresenceTestStop();if(!worker.Join(5000)){server.Dispose();worker.Join(1000);}Int(car+0x118,1218);Pointer(B+0x34b3e18,manager);Bytes(sim+0x28,new byte[]{0});}
   if(failure!=null)throw failure;Require(!worker.IsAlive,"Worker did not stop");
  }
 }
 public static string CheckIpc(){
  string name="fh2-native-test-"+Guid.NewGuid().ToString("N");Exception failure=null;
  using(var server=new NamedPipeServerStream(name,PipeDirection.InOut,1,PipeTransmissionMode.Byte,PipeOptions.None,4096,4096)){
   var thread=new Thread(delegate(){try{
    server.WaitForConnection();Require(Receive(server,0).Contains("1559239981548052682"),"Shared application ID missing");
    byte[] ready=Frame(1,"{\"cmd\":\"DISPATCH\",\"evt\":\"READY\"}");server.Write(ready,0,4);server.Flush();Thread.Sleep(50);server.Write(ready,4,ready.Length-4);server.Flush();
    string active=Receive(server,1);Require(active.Contains("SET_ACTIVITY")&&active.Contains("BMW"),"Native activity not sent");
    byte[] ping=Frame(3,"native-ping");server.Write(ping,0,ping.Length);server.Flush();Require(Receive(server,4)=="native-ping","Native pong mismatch");
    Ack(server,active);
    Require(Receive(server,1).Contains("\"activity\":null"),"Native clear not sent");
   }catch(Exception e){failure=e;}});thread.IsBackground=true;thread.Start();
   try {Require(Fh2PresenceTestSession("\\\\.\\pipe\\"+name,"{\"details\":\"BMW test\"}")==1,"Native session test failed");}
   finally {if(!thread.Join(5000)){server.Dispose();thread.Join(1000);}}
   if(failure!=null)throw failure;Require(!thread.IsAlive,"Mock thread did not finish");
  }
  return "PASS: native IPC handshake, fragmented frame, activity, ping/pong, acknowledgement, clear";
 }
 public static string CheckRejectedReplies(){
  foreach(string scenario in new[]{"wrong-nonce","nested-ready","malformed","oversize","error","closed","invalid-opcode"}){
   string name="fh2-reject-test-"+Guid.NewGuid().ToString("N");Exception failure=null;
   using(var server=new NamedPipeServerStream(name,PipeDirection.InOut,1,PipeTransmissionMode.Byte,PipeOptions.Asynchronous,4096,4096)){
    var thread=new Thread(delegate(){try{
     Require(server.WaitForConnectionAsync().Wait(5000),"Reject server connection timeout");
     if(scenario=="wrong-nonce") {Ready(server);Receive(server,1);Send(server,1,"{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"wrong\",\"evt\":null}");}
     else {Receive(server,0);
      if(scenario=="nested-ready")Send(server,1,"{\"data\":{\"evt\":\"READY\",\"cmd\":\"DISPATCH\"}}");
      if(scenario=="malformed")Send(server,1,"{\"evt\":\"READY\",\"cmd\":\"DISPATCH\"}junk");
      if(scenario=="oversize"){byte[] header=new byte[8];Array.Copy(BitConverter.GetBytes(1),header,4);Array.Copy(BitConverter.GetBytes(65536),0,header,4,4);server.Write(header,0,8);server.Flush();}
      if(scenario=="error")Send(server,1,"{\"evt\":\"ERROR\",\"data\":{\"message\":\"Rejected\\nrequest\"}}");
      if(scenario=="invalid-opcode")Send(server,9,"{}");
      if(scenario=="closed"){server.Disconnect();return;}
     }
     byte[] buffer=new byte[8];var read=server.ReadAsync(buffer,0,8);Require(read.Wait(5000)&&read.Result==0,"Rejected reply led to outgoing activity: "+scenario);
    }catch(Exception e){failure=e;}});thread.IsBackground=true;thread.Start();
    try{Require(Fh2PresenceTestSession("\\\\.\\pipe\\"+name,"{\"details\":\"test\"}")==0,"Invalid reply was accepted: "+scenario);}
    finally{if(!thread.Join(5000)){server.Dispose();thread.Join(1000);}}
    if(failure!=null)throw failure;Require(!thread.IsAlive,"Reject mock did not finish");
   }
  }
  return "PASS: wrong nonce, nested READY, malformed JSON, oversized frames, ERROR, disconnect and invalid opcode rejected";
 }
}
