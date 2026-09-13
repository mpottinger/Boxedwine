/* End-to-end ACM test for CheckACMAvail (retail 0x0048b823) and the
 * stream conversion used by FUN_00476288 (0x00476288). No retail assets.
 * A zero predictor and repeated IMA nibble 1 at step index 0 produce the
 * analytic ramp 0..1016, or unsigned 8-bit PCM 128+(sample>>8). */
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
void WINAPI audio_test_entry(void)
{
    IMAADPCMWAVEFORMAT source = {{WAVE_FORMAT_IMA_ADPCM,1,22050,11100,512,4,2},1017};
    WAVEFORMATEX target = {WAVE_FORMAT_PCM,1,22050,0,0,8,0};
    ACMSTREAMHEADER header = {sizeof(header)};
    HACMDRIVERID driver;
    HACMDRIVER opened;
    HMODULE module;
    HACMSTREAM stream;
    BYTE compressed[512], pcm[2048];
    DWORD written, result;
    const char *message = "ACM REGRESSION FAIL\n";
    for (int i=0;i<512;i++) compressed[i] = i < 4 ? 0 : 0x11;
    module = LoadLibraryA("C:\\windows\\system32\\imaadp32.acm");
    if (!module) { result=GetLastError();goto done; }
    result = acmDriverAddA(&driver, module, (LPARAM)GetProcAddress(module,"DriverProc"),0,ACM_DRIVERADDF_FUNCTION | ACM_DRIVERADDF_LOCAL);
    if (result) goto done;
    result = acmDriverOpen(&opened, driver, 0);
    if (result) goto done;
    result = acmFormatSuggest(opened,&source.wfx,&target,sizeof(target),0xf0000);
    if (result) goto done;
    result = acmStreamOpen(&stream,opened,&source.wfx,&target,NULL,0,0,0);
    if (result) goto done;
    header.pbSrc = compressed; header.cbSrcLength = sizeof(compressed);
    header.pbDst = pcm; header.cbDstLength = sizeof(pcm);
    result = acmStreamPrepareHeader(stream,&header,0);
    if (result) goto done;
    result = acmStreamConvert(stream,&header,ACM_STREAMCONVERTF_BLOCKALIGN);
    if (result) goto done;
    if (header.cbSrcLengthUsed != 512 || header.cbDstLengthUsed != 1017) { result=20;goto done; }
    for (unsigned i=0;i<1017;i++) if(pcm[i] != 128+(i>>8)) {result=21;goto done;}
    acmStreamUnprepareHeader(stream,&header,0);
    acmStreamClose(stream,0);
    acmDriverClose(opened,0);
    acmDriverRemove(driver,0);
    message = "ACM REGRESSION PASS: IMA ADPCM to 1017 verified PCM samples\n";
    result = 0;
done:
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),message,lstrlenA(message),&written,NULL);
    if (result) {
        char code[] = "ACM result=00000000\n";
        for(int i=0;i<8;i++)code[11+i]="0123456789abcdef"[(result>>((7-i)*4))&15];
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),code,sizeof(code)-1,&written,NULL);
    }
    ExitProcess(result?1:0);
}
