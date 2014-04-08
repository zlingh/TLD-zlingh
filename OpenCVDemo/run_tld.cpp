#include <opencv2/opencv.hpp>
#include "tld_utils.h"
#include "iostream"
#include "sstream"
#include "tld.h"
#include "stdio.h"
#include <time.h>
//#include <Windows.h>
using namespace cv;
using namespace std;
#undef UNICODE // ����㲻֪��ʲô��˼���벻Ҫ�޸�
//Global variables
#include <stdlib.h>
#include <Windows.h>
Rect box;
bool drawing_box = false;
bool gotBB = false;
bool tl = true;
bool rep = false;
bool fromfile=false;
bool fromCa=false;
long totalFrameNumber;
bool isImage=false;
char **imageList=NULL;
int listCount=0;
string video;
//���������ʾ����̨��ȡ�������ע��
#pragma comment( linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" )
//�����������directory���Ŀ¼�µ������ļ�����������׺������һ��ָ�������У�ָ��������ÿһ����һ��char�͵�ָ�룬
//���ص��Ǹ�ָ�룬���������ָ��������׵�ַ��������char*����һ��4���ֽڵ��ڴ��ַ��*ans�õ�����һ��ָ��cha��ָ�롣
char** EnumFiles(const char *directory, int *count)
{
	int direcLen=0;//directory����
	while(*(directory+(++direcLen))!=0);
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	int resultSize=500;
//	char result[MAX_RESULT][MAX_PATH];
	char *result=(char*)malloc(resultSize*MAX_PATH);// �����һ����̬�����ڴ�Ĺ��ܣ��Ȳ��˷��ֹ���
	char **returnresult;
	char pattern[MAX_PATH];
	int i = 0, j;
	// ��ʼ����
	strcpy(pattern, directory);
	strcat(pattern, "\\*.*");
	hFind = FindFirstFile(pattern, &FindFileData);

	if (hFind == INVALID_HANDLE_VALUE) 
	{
		*count = 0;
		return NULL;
	} 
	else 
	{
		do
		{
			if(i==resultSize)
			{ 
				char *temp=(char*)malloc(resultSize*2*MAX_PATH);
				memcpy(temp,result,resultSize*MAX_PATH*sizeof(char));//strcpy(temp,result);//Ŷ����ֻ������\0ǰ���
				char *temp1=result;
				result=temp;
				free(temp1);
				resultSize*=2;
			}
			strcpy(result+i*MAX_PATH,directory);
			*(result+(i)*MAX_PATH+direcLen)='\\';
			strcpy(result+(i++)*MAX_PATH+direcLen+1, FindFileData.cFileName);
		}
		while (FindNextFile(hFind, &FindFileData) != 0);
	}

	// ���ҽ���
	FindClose(hFind);

	// ���Ƶ������
	returnresult = (char **)calloc(i, sizeof(char *));

	for (j = 0; j < i-2; j++)
	{
		returnresult[j] = (char *)calloc(MAX_PATH, sizeof(char));
		strcpy(returnresult[j], result+(j+2)*MAX_PATH);
	}

	*count = i-2;
	return returnresult;
}

void readBB(char* file){
	ifstream bb_file (file);
	string line;
	getline(bb_file,line);
	istringstream linestream(line);
	string x1,y1,x2,y2;
	getline (linestream,x1, ',');
	getline (linestream,y1, ',');
	getline (linestream,x2, ',');
	getline (linestream,y2, ',');
	int x = atoi(x1.c_str());// = (int)file["bb_x"];
	int y = atoi(y1.c_str());// = (int)file["bb_y"];
	int w = atoi(x2.c_str())-x;// = (int)file["bb_w"];
	int h = atoi(y2.c_str())-y;// = (int)file["bb_h"];
	box = Rect(x,y,w,h);
}
//bounding box mouse callback
void mouseHandler(int event, int x, int y, int flags, void *param){
	switch( event ){
	case CV_EVENT_MOUSEMOVE:
		if (drawing_box){
			box.width = x-box.x;
			box.height = y-box.y;
		}
		break;
	case CV_EVENT_LBUTTONDOWN:
		drawing_box = true;
		box = Rect( x, y, 0, 0 );
		break;
	case CV_EVENT_LBUTTONUP:
		drawing_box = false;
		if( box.width < 0 ){
			box.x += box.width;
			box.width *= -1;
		}
		if( box.height < 0 ){
			box.y += box.height;
			box.height *= -1;
		}
		gotBB = true;
		break;
	}
}

void print_help(char** argv){
	printf("use:\n     %s -p /path/parameters.yml\n",argv[0]);
	printf("-s    source video\n-b        bounding box file\n-tl  track and learn\n-r     repeat\n");
}
void read_options(int len, char** c,VideoCapture& capture,FileStorage &fs){
		for (int i=0;i<len;i++){
		if (strcmp(c[i],"-b")==0&&!fromCa){
			if (len>i){
				readBB(c[i+1]);
				gotBB = true;
			}
			else
				print_help(c);
		}
		if (strcmp(c[i],"-s")==0&&!fromCa){
			if (len>i){
				video = string(c[i+1]);//continue;
				capture.open(video);
				fromfile = true;
			}
			else
				print_help(c);

		}
		if (strcmp(c[i],"-p")==0){
			if (len>i){
				fs.open(c[i+1], FileStorage::READ);
			}
			else
				print_help(c);
		}
		if (strcmp(c[i],"-no_tl")==0){
			tl = false;
		}
		if (strcmp(c[i],"-r")==0){
			rep = true;
		}
		if(strcmp(c[i],"-im")==0){
			char *directory=c[i+1];
			imageList=EnumFiles(directory,&listCount);
			char *temp=new char[20];
			for(int i=0;i<20;i++)
			{
				if(imageList[0][i]!='\\')
				temp[i]=imageList[0][i];
				else
				{
					temp[i]='\\';
					temp[i+1]=0;
					break;
				}
			}
			//listCount=listCount-2;//��һ����.���ڶ�����..��ȥ�����㡣
		    isImage=true;
			fromfile = true;
			fromCa=false;
			temp=strcat(temp,"init.txt");
			if(strcmp(imageList[listCount-1],temp)==0)
			{
				listCount--;
				readBB(imageList[listCount]);
				gotBB = true;
			}
		}
	}
}

void read_optionsbackup(int argc, char** argv,VideoCapture& capture,FileStorage &fs){
	for (int i=0;i<argc;i++){
		if (strcmp(argv[i],"-b")==0&&!fromCa){
			if (argc>i){
				readBB(argv[i+1]);
				gotBB = true;
			}
			else
				print_help(argv);
		}
		if (strcmp(argv[i],"-s")==0&&!fromCa){
			if (argc>i){
				video = string(argv[i+1]);
				capture.open(video);
				fromfile = true;
			}
			else
				print_help(argv);

		}
		if (strcmp(argv[i],"-p")==0){
			if (argc>i){
				fs.open(argv[i+1], FileStorage::READ);
			}
			else
				print_help(argv);
		}
		if (strcmp(argv[i],"-no_tl")==0){
			tl = false;
		}
		if (strcmp(argv[i],"-r")==0){
			rep = true;
		}
	}
}
vector<string> splitEx(const string& src, string separate_character)
{
	vector<string> strs;

	int separate_characterLen = separate_character.size();//�ָ��ַ����ĳ���,�����Ϳ���֧���硰,,�����ַ����ķָ���
	int lastPosition = 0,index = -1;
	while (-1 != (index = src.find(separate_character,lastPosition)))
	{
		strs.push_back(src.substr(lastPosition,index - lastPosition));
		lastPosition = index + separate_characterLen;
	}
	string lastString = src.substr(lastPosition);//��ȡ���һ���ָ����������
	if (!lastString.empty())
		strs.push_back(lastString);//������һ���ָ����������ݾ����
	return strs;
}
int main(int argc, char * argv[]){
	int patcharray[6]={15,20,25,30,35};
	int minwind[3]={5,10,15};
	FILE *pfilezp;//=fopen("Record.txt","w");
	FILE *patchf;
	time_t start,end;
	double wholecost;
	struct tm *ptr;
	int retry;
	int startFrame=0;
	bool nopoint=true;//�Ƿ���ʾ��
	bool drawDec=false;//�Ƿ���ʾdetection�Ŀ��
	bool cameraAgain=false;
	bool breaknow=false;//Ϊ���˳���ѭ������ı���
	bool play=false;//�Ƿ��л���playģʽ	
	char *test[]={
		"-p parameters.yml -s car.mpg -b car.txt",
		"-p ../parameters.yml -s ../datasets/01_david/david.mpg -b ../datasets/01_david/init.txt",
		"-p ../parameters.yml -s ../datasets/02_jumping/jumping.mpg -b ../datasets/02_jumping/init.txt",
		"-p ../parameters.yml -s ../datasets/03_pedestrian1/pedestrian1.mpg -b ../datasets/03_pedestrian1/init.txt",
		"-p ../parameters.yml -s ../datasets/04_pedestrian2/pedestrian2.mpg -b ../datasets/04_pedestrian2/init.txt",
		"-p ../parameters.yml -s ../datasets/05_pedestrian3/pedestrian3.mpg -b ../datasets/05_pedestrian3/init.txt",
		"-p ../parameters.yml -s ../datasets/06_car/car.mpg -b ../datasets/06_car/init.txt",
		"-p ../parameters.yml -s ../datasets/07_motocross/motocross.mpg -b ../datasets/07_motocross/init.txt",
		//"-p ../parameters.yml -s ../datasets/08_volkswagen/volkswagen.mpg -b ../datasets/08_volkswagen/init.txt",
		"-p ../parameters.yml -s ../datasets/09_carchase/carchase.mpg -b ../datasets/09_carchase/init.txt",
		"-p ../parameters.yml -s ../datasets/10_panda/panda.mpg -b ../datasets/10_panda/init.txt",
		"-p ../parameters.yml -s ../datasets/11_test/test2.avi"};
	char *testt[]={"-p parameters.yml -im data"};//,"-p parameters.yml -s car.mpg -b init1.txt",
		//"-p parameters.yml -s test.avi",
	//	"-p parameters.yml -s motocross.mpg -b init2.txt"};
	for(int i=0;i<1;i++){
		for (int flag=0;flag<1;flag++)
		//for (int pi=0;pi<15;pi++)		
		{
			RNG RNG( int64 seed=-1 );
			double costsum[7]={0.0,0.0,0.0,0.0,0.0,0.0,0.0};
			if(flag==1)
				int tempp=1;
			isImage=false;
			breaknow=false;
			retry=-1;
			patchf=fopen("patchgpu.txt", "at");
			pfilezp=fopen("Record.txt","at");					
			drawing_box = false;
			gotBB = false;
			tl = true;
			rep = false;
			fromfile=false;
			start=time(NULL); ptr=localtime(&start);
			printf(asctime(ptr));
			fprintf(pfilezp,asctime(ptr));
			wholecost = (double)getTickCount();
			VideoCapture capture;
			//CvCapture* capture;
			capture.open(1);
			//capture = cvCaptureFromCAM( CV_CAP_ANY);
			FileStorage fs;
			//Read options
			string s = test[flag];
			string del = " ";
			char test2[10][100];
			test2[4][0]='0';//�����к���ֵ����飬�´�ѭ��ʱ��Ȼ�������ϴ�ѭ����test2��ֵ������˵test2����ѭ�����涨��ģ�Ӧ���Ǹ��ֲ�������ÿ��ѭ��Ӧ�����¿��ı�������
			vector<string> strs = splitEx(s, del);
			for ( unsigned int i = 0; i < strs.size(); i++)
			{  
				//  cout << strs[i].c_str() << endl;
				//	test2[i]=strs[i].c_str();
				strcpy(test2[i],strs[i].c_str());
				//cout<<test2[i]<<endl;
			}
			//int tp=strs.size();
			char *p[10];
			char **test3;//�㲻̫�����ﰡ������
			for(int i=0;i<10;i++)
				p[i]=test2[i];
			test3=p; 	

			read_options(10,test3,capture,fs);

//			video = string(argv[1]);//Ŀ����Ƶ//ʵ���������������������
//			capture.open(video);
//			readBB(argv[2]);//Ŀ���


			// read_options(argc,argv,capture,fs);
			if(startFrame>0)//˵��������r����Ҫ���������ֶ�ѡ����
			{				
				box = Rect( 0, 0, 0, 0 );
				gotBB=false;
			}
			//   read_options(argc,argv,capture,fs);
			//Init camera
			if (!capture.isOpened()&&!isImage)//�򲻿���Ƶ���Ҳ���ͼ������
			{
				cout << "capture device failed to open!" << endl;
				return 1;
			}
			//Register mouse callback to draw the bounding box
			cvNamedWindow("TLD",CV_WINDOW_AUTOSIZE);
			cvSetMouseCallback("TLD", mouseHandler, NULL);
			//TLD framework
			TLD tld;
			//Read parameters file
			tld.read(fs.getFirstTopLevelNode());
//			tld.patch_size=atoi(argv[3]);
//			tld.min_win=atoi(argv[4]);	
			Mat frame;
			Mat last_gray;
			Mat first;
			if(fromCa)
			fromfile=false;
			fromCa=false;
			if (fromfile){
				if(!isImage){
					//	capture >> frame;
					totalFrameNumber = capture.get(CV_CAP_PROP_FRAME_COUNT);  
					cout<<"������Ƶ��"<<totalFrameNumber<<"֡"<<endl;
					//	capture.set( CV_CAP_PROP_POS_FRAMES,0); �ƺ�û����
					for(int i=0;i<=startFrame;i++){
						capture.read(frame);}
					cvtColor(frame, last_gray, CV_RGB2GRAY);
					frame.copyTo(first);
				}
				else{
					totalFrameNumber = listCount;  
					cout<<"����ͼ�����й�"<<listCount<<"֡"<<endl;
					//	capture.set( CV_CAP_PROP_POS_FRAMES,0); �ƺ�û����
					frame=imread(imageList[startFrame+2]);
					cvtColor(frame, last_gray, CV_RGB2GRAY);
					frame.copyTo(first);
				}

			}else{
				capture.set(CV_CAP_PROP_FRAME_WIDTH,340);
				capture.set(CV_CAP_PROP_FRAME_HEIGHT,240);
			}

			///Initialization
GETBOUNDINGBOX:
			while(!gotBB)
			{
				if (!fromfile){
					capture >> frame;
				}
				else
					first.copyTo(frame);
				cvtColor(frame, last_gray, CV_RGB2GRAY);
				drawBox(frame,box);
				imshow("TLD", frame);
				int cw=cvWaitKey(1);
				if (cw == 'q')
					return 0;
				if(cw=='p')
				{
					play=true;box=Rect( 0, 0, 15, 15 );
					break;
				}
			}
			if (min(box.width,box.height)<(int)fs.getFirstTopLevelNode()["min_win"]){
				cout << "Bounding box too small, try again." << endl;
				gotBB = false;
				goto GETBOUNDINGBOX;
			}
			//Remove callback
			cvSetMouseCallback( "TLD", NULL, NULL );
			printf("Initial Bounding Box = x:%d y:%d h:%d w:%d\n",box.x,box.y,box.width,box.height);

			//Output file
			FILE  *bb_file = fopen("bounding_boxes.txt","w");
			//fprintf(tablef,"%s\n",test2[3]);
			//TLD initialization
			tld.init(last_gray,box,bb_file);
			///Run-time
			Mat current_gray;
			BoundingBox pbox;
			vector<Point2f> pts1;
			vector<Point2f> pts2;
			bool status=true;
			int frames = 1;
			int detections = 1;
			int flagg=startFrame;//��¼�ǵڼ�֡

			// pfilezp=fopen("Record.txt","w");
REPEAT:     
			//	capture.set( CV_CAP_PROP_POS_FRAMES,startFrame);  			
			while((!isImage&&capture.read(frame))||(isImage)){
				if(isImage){					
					frame=imread(imageList[startFrame++]);
					if(startFrame>listCount-1){
						box=Rect( 0, 0, 0, 0 );
						break;}
				}
				
				flagg++;
				double cost = (double)getTickCount();
				//get frame
				cvtColor(frame, current_gray, CV_RGB2GRAY);
				//Process Frame  				
				if(!play)
					tld.processFrame(last_gray,current_gray,pts1,pts2,pbox,status,tl,bb_file,costsum);  
				//Draw Points
				if (status&&!play){
					if(!nopoint){
						drawPoints(frame,pts1);
						drawPoints(frame,pts2,Scalar(0,255,0));
					}
					drawBox(frame,pbox,Scalar(255,255,255),2);
					detections++;
				}
				if(drawDec){
				//	for(int j=0;j<tld.dt.bb.size();j++)
				//		drawBox(frame,tld.grid[tld.dt.bb[j]]);
					for(int j=0;j<tld.dbb.size();j++)//�˴�Ϊ��������ͼƬ��д���Ű�dbb����ʾ��������Ϊʹ�ù������㷨�Ժ�����dbb��
						drawBox(frame,tld.dbb[j]);
				}
				//Display
				imshow("TLD", frame);
				//swap points and images
				swap(last_gray,current_gray);
				pts1.clear();
				pts2.clear();
				frames++;
				if(frames==tld.pausenum) system("pause");				
				printf("Detection rate: %d/%d\n",detections,frames);
				if(frames==totalFrameNumber)
					tld.islast=true;
				cost=getTickCount()-cost;
				printf("--------------------------------process cost %gms\n", cost*1000/getTickFrequency());
				int c = waitKey(1);
				//int c= 'm';
				if(cameraAgain)
					c='c';//�����cameraģʽ�°�����r�����ص����
				switch(c){
				case 'n'://������һ����Ƶ
					{				
					//	retry==1;
						breaknow=true; 
						gotBB=false;
						box = Rect(0, 0, 0, 0);
						break;
					}
				case 'q':{
					return 0;}
				case 'r'://Ҫ�ֶ��ڵ�ǰ֡����ѡ��Ŀ����
					{
						if(fromfile)
						{
						if(play)
						startFrame=flagg;
						else
							startFrame=flagg-1;
						play=false;
						flag--;
						retry=1;						
						breaknow=true;
						break;
						}
						else{//�������ͷģʽʱ����r������ѡ��Ŀ����������൱�����ٴδ�Camģʽ
						cameraAgain=true;
						break;
						}
					}		
				case 'x':
					{
						nopoint=!nopoint;
						break;
					}
				case 'd':
					{
						drawDec=!drawDec;
						break;
					}
				case 'p':
					{
						play=!play;
						break;
					}
				case 's':
					{
						//�������򿪻��߹رտ���̨
						//#pragma comment( linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" )
					}
				case 'c'://�л���ʹ������ͷģʽ
					{
						cameraAgain=false;
						breaknow=true;
						fromCa=true;//readoptions��������õ�����жϣ����fromCaΪtrue����ʹ���ļ�Ҳ�����൱��ֻ������"-p ../parameters.yml"
						retry=1;
						box=Rect( 0, 0, 0, 0 );
						flag--;//ѭ�����˻�ȥ,���ǲ���
						break;
					}

				}
				if(breaknow) break;
				// if(flagg>=9)
				// fclose(pfilezp);
			} 			
			fprintf(pfilezp,"num=%d %s\n",flag,test2[3]);
			fprintf(pfilezp,"patch_size=%d\n",tld.patch_size);
			fprintf(pfilezp,"USE GPU OR CPU��%s\n",tld.use);
			fprintf(pfilezp,"Initial Bounding Box = x:%d y:%d h:%d w:%d\n",box.x,box.y,box.width,box.height);
			fprintf(pfilezp,"Detection rate: %d/%d\n",detections,frames);
			fprintf(pfilezp,"classifier.pEx: %d classifier.nEx: %d\n", tld.classifier.pEx.size(),tld.classifier.nEx.size());
			fclose(bb_file);
			wholecost=(getTickCount()-wholecost);
			end=time(NULL);
			fprintf(pfilezp,"timecost = %gms \n",wholecost*1000/getTickFrequency());
			fprintf(pfilezp,"every frame cost %g ms \n",wholecost*1000/getTickFrequency()/frames);
			fprintf(pfilezp,"%gms %gms %gms\n\n",costsum[0]/frames,costsum[1]/frames,costsum[2]/frames);
			//���δ�ӡ��Ƶ����/�����/patch��/patch��/��ʱ��/filter1/filter2/detect/track/learn/filter1���ݿ���ʱ��/filter2���ݿ���ʱ��
			fprintf(patchf,"%s\t %d/%d\t%d\t%d\t%g\t%g\t%g\t%g\t%g\t%g\t%g\t%g\n",argv[1],detections,frames,tld.patch_size,tld.min_win,wholecost*1000/getTickFrequency()/frames,costsum[0]/frames,costsum[1]/frames,costsum[2]/frames,costsum[3]/frames,costsum[4]/frames,costsum[5]/frames,costsum[6]/frames);
			//time_t start,end;
			//start=time(NULL); ptr=localtime(&start); printf(asctime(ptr));	 
			//fprintf(pfilezp,"timecost2=%f ms\n",difftime(end,start)*1000);
			fclose(pfilezp);	
			//fclose(tablef);
			fclose(patchf);
			if(retry==1)
			{
				continue;
			}//startFrame������
			startFrame=0;//����startFrame			
			if (rep){
				rep = false;
				tl = false;
				fclose(bb_file);
				bb_file = fopen("final_detector.txt","w");
				//capture.set(CV_CAP_PROP_POS_AVI_RATIO,0);
				capture.release();
				capture.open(video);
				goto REPEAT;
			} 
		}
	}
	//  fclose(pfilezp);
	// fclose(pfilezpp);
	//char c=getchar();
	return 0;
}