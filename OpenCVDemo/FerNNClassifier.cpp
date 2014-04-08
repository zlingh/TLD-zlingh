/*
 * FerNNClassifier.cpp
 *
 *  Created on: Jun 14, 2011
 *      Author: alantrrs
 */

#include "FerNNClassifier.h"

using namespace cv;
using namespace std;

void FerNNClassifier::read(const FileNode& file){
  ///Classifier Parameters
  valid = (float)file["valid"];
  ncc_thesame = (float)file["ncc_thesame"];
  nstructs = (int)file["num_trees"];
  structSize = (int)file["num_features"];
  thr_fern = (float)file["thr_fern"];
  thr_nn = (float)file["thr_nn"];
  thr_nn_valid = (float)file["thr_nn_valid"];
  nExLimit=(int)file["nExLimit"];
  pExLimit=(int)file["pExLimit"];
  nExTimes=0;
  pExTimes=0;
  nExInfo=new ExInfo[nExLimit];
  pExInfo=new ExInfo[pExLimit];
}

void FerNNClassifier::prepare(const vector<Size>& scales){
  acum = 0;
  //Initialize test locations for features
  int totalFeatures = nstructs*structSize;
  features = vector<vector<Feature> >(scales.size(),vector<Feature> (totalFeatures));
  RNG& rng = theRNG();
  float x1f,x2f,y1f,y2f;
  int x1, x2, y1, y2;
  for (int i=0;i<totalFeatures;i++){
      x1f = (float)rng;
      y1f = (float)rng;
      x2f = (float)rng;
      y2f = (float)rng;
      for (int s=0;s<scales.size();s++){
          x1 = x1f * scales[s].width;
		  //���Կ�Ⱥ�õ��˵ڼ��У�������ferNNclassifier������patch.at<uchar>(y1,x1) > patch.at<uchar>(y2, x2);
		  //important,patch.at<uchar>(y1,x1)�õ����ǵ�y1�У���x1��
          y1 = y1f * scales[s].height;//��y1��
          x2 = x2f * scales[s].width;//
          y2 = y2f * scales[s].height;
          features[s][i] = Feature(x1, y1, x2, y2);
      }
  }
  //Thresholds
  thrN = 0.5*nstructs;

  //Initialize Posteriors
  for (int i = 0; i<nstructs; i++) {
      posteriors.push_back(vector<float>(pow(2.0,structSize), 0));
      pCounter.push_back(vector<int>(pow(2.0,structSize), 0));
      nCounter.push_back(vector<int>(pow(2.0,structSize), 0));
  }
}

void FerNNClassifier::getFeatures(const cv::Mat& image,const int& scale_idx, vector<int>& fern){
  int leaf;
  for (int t=0;t<nstructs;t++){
      leaf=0;
      for (int f=0; f<structSize; f++){
          leaf = (leaf << 1) + features[scale_idx][t*structSize+f](image);
      }
      fern[t]=leaf;
  }
}

float FerNNClassifier::measure_forest(vector<int> fern) {
  float votes = 0;
  for (int i = 0; i < nstructs; i++) {
      votes += posteriors[i][fern[i]];
  }
  return votes;
}

void FerNNClassifier::update(const vector<int>& fern, int C, int N) {
  int idx;
 // float *upPos=new float[nstructs*2];//ǰnstructs��ʽfernֵ��������posteriorsֵ
  float *upPos=new float[nstructs];
  int *upPosInd=new int [nstructs];
  for (int i = 0; i < nstructs; i++) {
      idx = fern[i];
      (C==1) ? pCounter[i][idx] += N : nCounter[i][idx] += N;
      if (pCounter[i][idx]==0) {
          posteriors[i][idx] = 0;
		 // upPos[i]=idx;		 
		 // upPos[i+nstructs]=0;
		   upPosInd[i]=idx;
		    upPos[i]=0;
		  //����Ҳ�и���������tld.initʱ��������ݾͻ���£�����ʱ�����ǵ�gpu��û���κ����ݰ�
		  //�����posteriors�Ĵ�С�趨����tld.initǰ����ʱ�����޷�֪��nstructs�Ĵ�С.
		  //�ã�tld��nstructs�������ļ������ʱ��������ڹ��캯����������Ǿ���gpu�ͣ���ô��
		  //Ҳ����˵����Щ���������÷��빹�캯����
      } else {
          posteriors[i][idx] = ((float)(pCounter[i][idx]))/(pCounter[i][idx] + nCounter[i][idx]);
		//  upPos[i]=idx;
		//  upPos[i+nstructs]=posteriors[i][idx];
		   upPosInd[i]=idx;
		    upPos[i]=posteriors[i][idx];
      }
  }
    delete[] upPos;
   delete[] upPosInd;
}

void FerNNClassifier::trainF(const vector<std::pair<vector<int>,int> >& ferns,int resample){
  // Conf = function(2,X,Y,Margin,Bootstrap,Idx)
  //                 0 1 2 3      4         5
  //  double *X     = mxGetPr(prhs[1]); -> ferns[i].first
  //  int numX      = mxGetN(prhs[1]);  -> ferns.size()
  //  double *Y     = mxGetPr(prhs[2]); ->ferns[i].second
  //  double thrP   = *mxGetPr(prhs[3]) * nTREES; ->threshold*nstructs
  //  int bootstrap = (int) *mxGetPr(prhs[4]); ->resample
  float temp=0;
  thrP = thr_fern*nstructs;                                                          // int step = numX / 10;
  //for (int j = 0; j < resample; j++) {                      // for (int j = 0; j < bootstrap; j++) {
      for (int i = 0; i < ferns.size(); i++){               //   for (int i = 0; i < step; i++) {
                                                            //     for (int k = 0; k < 10; k++) {
                                                            //       int I = k*step + i;//box index
                                                            //       double *x = X+nTREES*I; //tree index
          if(ferns[i].second==1){ //1������good_box                          //       if (Y[I] == 1) {
              if(measure_forest(ferns[i].first)<=thrP)      //         if (measure_forest(x) <= thrP)
                update(ferns[i].first,1,1);                 //             update(x,1,1);
          }else{                                            //        }else{
              if ((temp=measure_forest(ferns[i].first)) >= thrN){   //         if (measure_forest(x) >= thrN)
				//measure_forest(ferns[i].first);
				  update(ferns[i].first,0,1);   }              //             update(x,0,1);
          }
      }
  //}
}
void FerNNClassifier::trainNN(const vector<cv::Mat>& nn_examples){
  float conf,dummy;
  vector<int> y(nn_examples.size(),0);
  y[0]=1;
  vector<int> isin;
  for (int i=0;i<nn_examples.size();i++){                          //  For each example
      NNConf(nn_examples[i],isin,conf,dummy);                      //  Measure Relative similarity
      if (y[i]==1 && conf<=thr_nn){                                //    if y(i) == 1 && conf1 <= tld.model.thr_nn % 0.65
          if (isin[1]<0){                                          //      if isnan(isin(2))
             //1 pEx = vector<Mat>(1,nn_examples[i]);                 //        tld.pex = x(:,i);
			 //1 addpExgpu(nn_examples[i]);
			  addpEx(nn_examples[i],conf,pExTimes);
		  pExTimes++;
              continue;                                            //        continue;
          }                                                        //      end
          //////////////pEx.insert(pEx.begin()+isin[1],nn_examples[i]);        //      tld.pex = [tld.pex(:,1:isin(2)) x(:,i) tld.pex(:,isin(2)+1:end)]; % add to model
		  //pEx.push_back(nn_examples[i]);
		  //addpExgpu(nn_examples[i]);
		  addpEx(nn_examples[i],conf,pExTimes);
		  pExTimes++;
      }                                                            //    end
      if(y[i]==0 && conf>0.5)                                      //  if y(i) == 0 && conf1 > 0.5
	  {		  
		  addnEx(nn_examples[i],conf,nExTimes);
		  nExTimes++;
	//1	nEx.push_back(nn_examples[i]);                             //    tld.nex = [tld.nex x(:,i)];
	//1    addnExgpu(nn_examples[i]);
	  }
  }                                                                 //  end
  acum++;
  printf("%d. Trained NN examples: %d positive %d negative\n",acum,(int)pEx.size(),(int)nEx.size());
}                                                                  //  end

void opcvNcc_CCORR_NORMED(cv::Mat &src,const cv:: Mat &dst,float &ncc){	
	Mat ncctmp(1,1,CV_32F);
	matchTemplate(src,dst,ncctmp,CV_TM_CCORR_NORMED);     //measure NCC to negative examples
	ncc=((float*)ncctmp.data)[0];
}

void myNcc_CCORR_NORMED(cv::Mat &src,const cv:: Mat &dst,float &ncc){	//���ǹ�һ�����ƥ�䷨��method=CV_TM_CCORR_NORMED���Զ��ж���8U����32F
	if(src.step[1]==1){//ʵ����ָsrc.step.buf������ֵ����һ��src.step[0]�Ǿ�����п���λ���ֽڣ����ڶ����Ǿ�����������͵Ĵ�С���ֽڵ�λ��
		double srcCFH=0.0,dstCFH=0.0,nccSum=0.0;
		for (int i=0;i<src.rows;i++)
			for (int j=0;j<src.cols;j++)
			{
				srcCFH+=(src.at<uchar>(i,j))*(src.at<uchar>(i,j));
				dstCFH+=(dst.at<uchar>(i,j))*(dst.at<uchar>(i,j));
				nccSum+=(src.at<uchar>(i,j))*(dst.at<uchar>(i,j));
			}
			double CFH=sqrt((double)srcCFH)*sqrt((double)dstCFH);
			ncc=(double)nccSum/CFH;
	}
	else{

		double srcCFH=0.0,dstCFH=0.0,nccSum=0.0;
		for (int i=0;i<src.rows;i++)
			for (int j=0;j<src.cols;j++)
			{
				srcCFH+=(src.at<float>(i,j))*(src.at<float>(i,j));
				dstCFH+=(dst.at<float>(i,j))*(dst.at<float>(i,j));
				nccSum+=(src.at<float>(i,j))*(dst.at<float>(i,j));
			}
			double CFH=sqrt((double)srcCFH)*sqrt((double)dstCFH);
			ncc=(double)nccSum/CFH;
	}


}

void myNcc1(cv::Mat &src,cv:: Mat &dst,float &ncc){//����8U�Ĺ�һ�����ϵ��ƥ�䷨��method=CV_TM_CCOEFF_NORMED
	int srcSum=0,dstSum=0;
	int row=src.rows;
	int col=src.cols;
	for (int i=0;i<src.rows;i++)
		for (int j=0;j<src.cols;j++)
		{
			srcSum+=src.at<uchar>(i,j);
			dstSum+=dst.at<uchar>(i,j);
		}
		double srcAver=(double)srcSum/(src.rows*src.cols);
		double dstAver=(double)dstSum/(dst.rows*dst.cols);
		int srcCFH=0.0,dstCFH=0.0,nccSum=0.0;
		for (int i=0;i<src.rows;i++)
			for (int j=0;j<src.cols;j++)
			{
				srcCFH+=(src.at<uchar>(i,j)-srcAver)*(src.at<uchar>(i,j)-srcAver);
				dstCFH+=(dst.at<uchar>(i,j)-dstAver)*(dst.at<uchar>(i,j)-dstAver);
				nccSum+=(src.at<uchar>(i,j)-srcAver)*(dst.at<uchar>(i,j)-dstAver);
			}
			double CFH=sqrt((double)srcCFH)*sqrt((double)dstCFH);
			ncc=(double)nccSum/CFH;
			if(ncc<0) ncc=ncc*(-1);
}

void FerNNClassifier::NNConf(const Mat& example, vector<int>& isin,float& rsconf,float& csconf){
  /*Inputs:
   * -NN Patch
   * Outputs:
   * -Relative Similarity (rsconf), Conservative Similarity (csconf), In pos. set|Id pos set|In neg. set (isin)
   */
  isin=vector<int>(3,-1);
  if (pEx.empty()){ //if isempty(tld.pex) % IF positive examples in the model are not defined THEN everything is negative
      rsconf = 0; //    conf1 = zeros(1,size(x,2));
      csconf=0;
      return;
  }
  if (nEx.empty()){ //if isempty(tld.nex) % IF negative examples in the model are not defined THEN everything is positive
      rsconf = 1;   //    conf1 = ones(1,size(x,2));
      csconf=1;
      return;
  }
  Mat ncc(1,1,CV_32F);
  float nccP,csmaxP=0,maxP=0,myncc=0;
  bool anyP=false;
  int maxPidx=0,validatedPart = ceil(pEx.size()*valid);
  float nccN, maxN=0;
  bool anyN=false;
//   gpu::GpuMat gpu_scene, gpu_templ,gpu_result;	
//   	vector<Mat> rgbImg(3);	
//	vector<Mat> rgbImg2(3);
	Mat pExi,examplei,nExi;
//	example.convertTo(examplei,CV_8U);
//	gpu_templ.upload(examplei);
	//gpu_templ.upload(examplei);
  for (int i=0;i<pEx.size();i++){

	//Ҫʹ��gpu������Ҫ��ͼ���32fתΪ8u.
  
    //cvtColor(pEx[i],pExi,CV_RGB2GRAY);
    //cvtColor(example,examplei,CV_RGB2GRAY);
	//split(pEx[i], rgbImg);
	//Mat scene1chan=rgbImg[0];	
	//split(example, rgbImg2);
	//Mat templ1chan=rgbImg2[0];
	//gpu_scene.upload(scene1chan);
	//gpu_templ.upload(templ1chan);
//	pEx[i].convertTo(pExi,CV_8U);	
//	gpu_scene.upload(pExi);	
//	gpu_result.create(gpu_scene.rows-gpu_templ.rows+1 , gpu_scene.cols-gpu_templ.cols+1 , CV_32F);
//	gpu::matchTemplate(gpu_scene, gpu_templ, gpu_result,CV_TM_CCORR_NORMED);      // measure NCC to positive examples	  
//	ncc=Mat(gpu_result);
//	matchTemplate(pEx[i],example,ncc,CV_TM_CCORR_NORMED);
//	matchTemplate(pExi,examplei,ncc,CV_TM_CCORR_NORMED);
    //normalize( ncc, ncc, 0, 1, NORM_MINMAX, -1, Mat() );
//float	nccP1=(((float*)ncc.data)[0]+1)*0.5;//��ô����ԭ����CV_TM_CCORR_NORMED,��һ������׼�����ƥ��CV_TM_CCOEFF ���ϵ��ƥ�䷨��1��ʾ������ƥ�䣻-1��ʾ����ƥ�䡣
	 
	  
	  //myNcc_CCORR_NORMED(pEx[i],example,myncc);//ʹ���Լ���д�Ĵ���
	  opcvNcc_CCORR_NORMED(pEx[i],example,myncc);//ʹ��opcv�Ĵ���

	  nccP=(myncc+1)*0.5;
//	if(nccP1-nccP>0.0001||nccP1-nccP<-0.0001)
//	{
//		printf("p=%f,p1=%f",nccP,nccP1);system("PAUSE");
//	}
      if (nccP>ncc_thesame)
        anyP=true;
      if(nccP > maxP){
          maxP=nccP;
          maxPidx = i;
          if(i<validatedPart)
            csmaxP=maxP;
      }
  }

  for (int i=0;i<nEx.size();i++){
	  //myNcc_CCORR_NORMED(nEx[i],example,myncc);
	  opcvNcc_CCORR_NORMED(nEx[i],example,myncc);

	  nccN=(myncc+1)*0.5;
      if (nccN>ncc_thesame)
        anyN=true;
      if(nccN > maxN)
        maxN=nccN;
  }

  //set isin
  if (anyP) isin[0]=1;  //if he query patch is highly correlated with any positive patch in the model then it is considered to be one of them
  isin[1]=maxPidx;      //get the index of the maximall correlated positive patch
  if (anyN) isin[2]=1;  //if  the query patch is highly correlated with any negative patch in the model then it is considered to be one of them
  //Measure Relative Similarity
  float dN=1-maxN;
  float dP=1-maxP;
  rsconf = (float)dN/(dN+dP);
  //Measure Conservative Similarity
  dP = 1 - csmaxP;
  csconf =(float)dN / (dN + dP);
}

void FerNNClassifier::evaluateTh(const vector<pair<vector<int>,int> >& nXT,const vector<cv::Mat>& nExT){
float fconf;
  for (int i=0;i<nXT.size();i++){
    fconf = (float) measure_forest(nXT[i].first)/nstructs;
    if (fconf>thr_fern)
      thr_fern=fconf;
}
  vector <int> isin;
  float conf,dummy;
  for (int i=0;i<nExT.size();i++){
      NNConf(nExT[i],isin,conf,dummy);
      if (conf>thr_nn)
        thr_nn=conf;
  }
  if (thr_nn>thr_nn_valid)
    thr_nn_valid = thr_nn;
}

void FerNNClassifier::show(){
  Mat examples((int)pEx.size()*pEx[0].rows,pEx[0].cols,CV_8U);
  double minval;
  Mat ex(pEx[0].rows,pEx[0].cols,pEx[0].type());
  for (int i=0;i<pEx.size();i++){
    minMaxLoc(pEx[i],&minval);
    pEx[i].copyTo(ex);
    ex = ex-minval;
    Mat tmp = examples.rowRange(Range(i*pEx[i].rows,(i+1)*pEx[i].rows));
    ex.convertTo(tmp,CV_8U);
	// imshow("tmp",tmp);
  }
 // printf("pEx NUMBER is: %d\n", pEx.size());
  imshow("Examples",examples);
}
//�������һ��ָ���������ָ�������Сdetections�У�1�С�������ǽ�����ݴ���nccBatAns,���Ǹ���detections��pEx.size()+nEx.size()�еĶ�ά����
int FerNNClassifier:: getnExPosition()
{
    //����ɵ�һ�����ƶ���С��ɾ��	
    //length=sizeof(nExInfo)/sizeof(ExInfo);

	int *ban=new int[nExLimit/2];

	for (int i=0;i<nExLimit;i++)
	{
		if (i<nExLimit/2)
		{
			ban[i]=i;continue;
		}
		int maxt=0;
		for (int j=1;j<nExLimit/2;j++)
		{
			if(nExInfo[ban[j]].times>nExInfo[ban[maxt]].times)
				maxt=j;
		}//newt����ban��nExInfo[ban[newt]]����
		//��������������������С�ģ����滻
		if (nExInfo[i].times<nExInfo[ban[maxt]].times)
			ban[maxt]=i;


	}
	//����ban������times��С��һ��nExInfo���±꣬Ȼ�������ѡһ��conf��С���±����
	int minn=0;//����Ҫ����ʱ����С���±�
	for(int i=1;i<nExLimit/2;i++)
	{
		
		if(nExInfo[ban[i]].conf<nExInfo[ban[minn]].conf)
			minn=i;
	}
	return ban[minn];

}

int FerNNClassifier:: getpExPosition()
{
    //����ɵ�һ�����ƶ�����ɾ��	
    //length=sizeof(pExInfo)/sizeof(ExInfo);

	int *ban=new int[pExLimit/2];

	for (int i=0;i<pExLimit;i++)
	{
		if (i<pExLimit/2)
		{
			ban[i]=i;continue;
		}
		int maxt=0;
		for (int j=1;j<pExLimit/2;j++)
		{
			if(pExInfo[ban[j]].times>pExInfo[ban[maxt]].times)
				maxt=j;
		}//newt����ban��pExInfo[ban[newt]]����
		//��������������������С�ģ����滻
		if (pExInfo[i].times<pExInfo[ban[maxt]].times)
			ban[maxt]=i;


	}
	//����ban������times��С��һ��nExInfo���±꣬Ȼ�������ѡһ��conf�����±����
	int maxn=0;//����Ҫ����ʱ����С���±�
	for(int i=1;i<pExLimit/2;i++)
	{
		
		if(pExInfo[ban[i]].conf>pExInfo[ban[maxn]].conf)
			maxn=i;
	}
	return ban[maxn];

}


//��������Ĺ����ǣ�����һ��nex��mat,conf,�ڼ��������뵽nEx��
void FerNNClassifier::addnEx(const cv::Mat &n,float conf,int times)
{//����ɵ�һ�����ƶ�����ɾ��
	//����cpu
	int nExPosition;

	ExInfo exTemp={ times,conf};
	if(nEx.size()<nExLimit)//���nEx�Ĵ�СС�����ޣ���ֱ�Ӽ��뵽���棬gpu��Ҳ��
	{
		nEx.push_back(n);
		
		nExInfo[nEx.size()-1]=exTemp;
	}
	else
	{//�����ҵ���صط��滻
		nExPosition=getnExPosition();
		nEx[nExPosition]=n;
		
		nExInfo[nExPosition]=exTemp;
	}
}


void FerNNClassifier::addpEx(const cv::Mat &n,float conf,int times)
{//����ɵ�һ�����ƶ���С��ɾ��
	//����cpu
	int pExPosition;

	ExInfo exTemp={ times,conf};
	if(pEx.size()<pExLimit)//���pEx�Ĵ�СС�����ޣ���ֱ�Ӽ��뵽���棬gpu��Ҳ��
	{
		pEx.push_back(n);
		
		pExInfo[pEx.size()-1]=exTemp;
	}
	else
	{//�����ҵ���صط��滻
		pExPosition=getpExPosition();
		pEx[pExPosition]=n;
		
		pExInfo[pExPosition]=exTemp;
	}
}


