#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <sys/vfs.h>
#include "oledfont.h"

int fd; //WiringPi I2C Section Num
char DisplayBuffer[8][21];//Font Size is 8x6 -> 8Line x 21 Chars
int hour;
void InitOled()
{
	wiringPiSetup();
	fd=wiringPiI2CSetup(0x3C);//Init I2C, 0x3C is the address of SSD1306
	wiringPiI2CWriteReg8(fd,0x00,0xA1);//Change 0xA1 to 0xA0 to reverse display
	wiringPiI2CWriteReg8(fd,0x00,0xC8);//Change 0xC8 to 0xC0 to reverse line direction
	wiringPiI2CWriteReg8(fd,0x00,0x8D);
	wiringPiI2CWriteReg8(fd,0x00,0x14);
	wiringPiI2CWriteReg8(fd,0x00,0xA6);//Change 0xA6 to 0xA7 to reverse color
	wiringPiI2CWriteReg8(fd,0x00,0x21);
	wiringPiI2CWriteReg8(fd,0x00,0x00);
	wiringPiI2CWriteReg8(fd,0x00,0x7F);
	wiringPiI2CWriteReg8(fd,0x00,0xAF);
}
void ClearOled()
{
	int i,j;
	for(i=0;i<8;i++)
	{
		wiringPiI2CWriteReg8(fd,0x00,0xb0+i);
	    for(j=0;j<128;j++) 
			wiringPiI2CWriteReg8(fd,0x40,0x00); //Clear Display
	}
}
void ClearBuffer(int line) //line=-1 for all line
{
	int i,j;
	if(line==-1)
	{
		for(i=0;i<8;i++)
			for(j=0;j<21;j++)
				DisplayBuffer[i][j]=0x20; //Use Space to Clear Buffer
	}
	else
	{
		i=line;
		for(j=0;j<21;j++)
			DisplayBuffer[i][j]=0x20;
	}
}
void UpdateBuffer()
{
	int i,j,k;
	for(i=0;i<8;i++)
	{
		wiringPiI2CWriteReg8(fd,0x00,0xb0+i);
	    for(j=0;j<21;j++)
			for(k=0;k<6;k++)
				wiringPiI2CWriteReg8(fd,0x40,F6x8[DisplayBuffer[i][j]-0x20][k]);
		wiringPiI2CWriteReg8(fd,0x40,0x00); //Fit in 128 Byte
		wiringPiI2CWriteReg8(fd,0x40,0x00);
	}
}
void MyCharCopy(char *dst,char *src,int maxlen)
{
	int i;
	for(i=0;i<maxlen;i++)
	{
		if(*src==0)
			break;
		if(*src=='\n' || *src=='\r')
			*dst=0x32;
		else
			*dst=*src;
		dst++;src++;
	}
}
void GetSysTime()
{
	char tmpstr[30];
	struct tm *p;
	time_t timep;
	ClearBuffer(0);
	time(&timep);
	p=localtime(&timep);
	sprintf(tmpstr,"%d/%02d/%02d %02d:%02d",(1900+p->tm_year),(1+p->tm_mon),p->tm_mday,p->tm_hour,p->tm_min);
	MyCharCopy(DisplayBuffer[0],tmpstr,21);
	hour=p->tm_hour;
}
void GetTemperature()
{
	int fp;
	char tmpstr[30];int rawtemp;
	ClearBuffer(1);
	fp=open("/sys/class/thermal/thermal_zone0/temp",O_RDONLY);
	read(fp,tmpstr,5);
	close(fp);
	tmpstr[5]=0;
	rawtemp=atoi(tmpstr);
	sprintf(tmpstr,"Soc Temp:%d.%03d`C",rawtemp/1000,rawtemp%1000);
	MyCharCopy(DisplayBuffer[1],tmpstr,21);
}
void GetLoadAverage()
{
	FILE *fp;
	char tmpstr[30];
	char loadavg1[10],loadavg5[10],loadavg15[10];
	ClearBuffer(2);
	fp=fopen("/proc/loadavg","rt");
	fscanf(fp,"%s %s %s",loadavg1,loadavg5,loadavg15);
	fclose(fp);
	sprintf(tmpstr,"Ld:%s %s %s",loadavg1,loadavg5,loadavg15);
	MyCharCopy(DisplayBuffer[2],tmpstr,21);
}
void GetMeminfo()
{
	FILE *fp;
	char tmpstr[30];
	int memtotal=-1,memfree=-1,swaptotal=-1,swapfree=-1;
	char fpstr[30];int fpint;
	ClearBuffer(3);
	fp=fopen("/proc/meminfo","rt");
	while(1)
	{
		if(fscanf(fp,"%s %d kB",fpstr,&fpint)==-1)
			break;
		//printf("Data:%s %d\n",fpstr,fpint);
		if(memtotal==-1 && strcmp(fpstr,"MemTotal:")==0)
			memtotal=fpint;
		if(memfree==-1 && strcmp(fpstr,"MemFree:")==0)
			memfree=fpint;
		if(swaptotal==-1 && strcmp(fpstr,"SwapTotal:")==0)
			swaptotal=fpint;
		if(swapfree==-1 && strcmp(fpstr,"SwapFree:")==0)
			swapfree=fpint;
	}
	fclose(fp);
	if(swaptotal==0)
		sprintf(tmpstr,"FreeMem:%d%%",memfree*100/memtotal);
	else
		sprintf(tmpstr,"Mem:%d%% Swap:%d%%",memfree*100/memtotal,swapfree*100/swaptotal);
	MyCharCopy(DisplayBuffer[3],tmpstr,21);
}
void GetNetSpeed()
{
	FILE *fp;
	char tmpstr[40];
	char fpstr[100],fpc;
	long long int value[16];
	int linecount=0,i;
	static long long int lasttxb=0,lastrxb=0,lasttime=0;
	long long int nowtxb=0,nowrxb=0,nowtime=0;
	int txspeed,rxspeed;
	ClearBuffer(4);ClearBuffer(5);
	fp=fopen("/proc/net/dev","rt");
	while(linecount<2)
	{
		fscanf(fp,"%c",&fpc);
		if(fpc=='\n')
			linecount++;
	}
	while(linecount<3)
	{
		fscanf(fp,"%s",fpstr);
		for(i=0;i<16;i++)
			fscanf(fp,"%lld",&value[i]);
		if(strcmp(fpstr,"eth0:")==0)
		{
			linecount++;
			nowrxb=value[0];
			nowtxb=value[8];
		}
	}
	fclose(fp);
	nowtime=time((time_t*)NULL);
	if(lasttime==0)
	{
		sprintf(tmpstr,"RX:Gathering info");
		MyCharCopy(DisplayBuffer[4],tmpstr,21);
		sprintf(tmpstr,"TX:Gathering info");
		MyCharCopy(DisplayBuffer[5],tmpstr,21);
	}
	else
	{
		txspeed=(int)(nowtxb-lasttxb)/1024/(nowtime-lasttime);
		rxspeed=(int)(nowrxb-lastrxb)/1024/(nowtime-lasttime);
		sprintf(tmpstr,"RX:%dKB/s",rxspeed);
		MyCharCopy(DisplayBuffer[4],tmpstr,21);
		sprintf(tmpstr,"TX:%dKB/s",txspeed);
		MyCharCopy(DisplayBuffer[5],tmpstr,21);
	}
	lasttxb=nowtxb;
	lastrxb=nowrxb;
	lasttime=nowtime;
}
void GetSdcardUsage()
{
	FILE *fp;
	char tmpstr[40];
	struct statfs diskInfo;
	ClearBuffer(6);
	statfs("/", &diskInfo);
	unsigned long long totalBlocks = diskInfo.f_bsize;
	unsigned long long totalSize = totalBlocks * diskInfo.f_blocks;
	size_t mbTotalsize = totalSize>>20;
	unsigned long long freeDisk = diskInfo.f_bfree*totalBlocks;
	size_t mbFreedisk = freeDisk>>20;
	sprintf(tmpstr,"SD:%dM/%dM",mbFreedisk,mbTotalsize);
	MyCharCopy(DisplayBuffer[6],tmpstr,21);
}
void GetSDA1SDB1() //Can be modified to other function
{
	char tmpstr[30];
	int sda1free,sdb1free;
	struct statfs diskInfo;
	ClearBuffer(7);
	statfs("/mnt/sda1", &diskInfo);
	unsigned long long totalBlocks = diskInfo.f_bsize;
	unsigned long long totalSize = totalBlocks * diskInfo.f_blocks;
	size_t mbTotalsize = totalSize>>20;
	unsigned long long freeDisk = diskInfo.f_bfree*totalBlocks;
	size_t mbFreedisk = freeDisk>>20;
	sda1free=mbFreedisk*100/mbTotalsize;
	statfs("/mnt/sdb1", &diskInfo);
	totalBlocks = diskInfo.f_bsize;
	totalSize = totalBlocks * diskInfo.f_blocks;
	mbTotalsize = totalSize>>20;
	freeDisk = diskInfo.f_bfree*totalBlocks;
	mbFreedisk = freeDisk>>20;
	sdb1free=mbFreedisk*100/mbTotalsize;
	sprintf(tmpstr,"SDA1:%d%% SDB1:%d%%",sda1free,sdb1free);
	MyCharCopy(DisplayBuffer[7],tmpstr,21);
}
int main()
{
	int count=0;
	InitOled();
	ClearOled();
	ClearBuffer(-1);
	while(1)
	{
		//ClearBuffer();
		GetSysTime();
		GetTemperature();
		GetLoadAverage();
		GetMeminfo();
		GetNetSpeed();
		if(count%5==0) //Decrease Fresh Frequency
		{
			GetSdcardUsage();
			GetSDA1SDB1();
		}
		UpdateBuffer();
		count++;printf("Fresh Count:%d\n",count);
		sleep((hour>=23||hour<8)?60:5);
		
	}
}