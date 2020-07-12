#define BYTE unsigned char
#define USHORT unsigned short

unsigned short UpdateCrc(BYTE ch, USHORT *lpwCrc)
{
    ch = (ch^(BYTE)((*lpwCrc) & 0x00FF));
    ch = (ch^(ch<<4));
    *lpwCrc = (*lpwCrc >> 8)^((USHORT)ch <<8)^((USHORT)ch<<3)^((USHORT)ch>>4);
    return(*lpwCrc);
}
void ComputeCrc(char *Data, int Length, BYTE *TransmitFirst, BYTE *TransmitSecond)
{
    BYTE chBlock;
    USHORT wCrc;
    wCrc = 0xFFFF; // ISO 3309
    do
    {
        chBlock = *Data++;
        UpdateCrc(chBlock, &wCrc);
    } while (--Length);
    wCrc = ~wCrc; // ISO 3309
    *TransmitFirst = (BYTE) (wCrc & 0xFF);
    *TransmitSecond = (BYTE) ((wCrc >> 8) & 0xFF);
    return;
}