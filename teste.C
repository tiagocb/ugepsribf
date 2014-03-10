#include <charm++.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include "teste.decl.h"

#define DEBUG									1

#define PRINT_TASKS_PUS				0
#define PRINT_TASKS_WORK_TIME	0
#define PRINT_TASKS_OPTYPE		0
#define PRINT_TASKS_LOAD			0
#define PRINT_TASKS_SIZE			0
#define PRINT_TASKS_SENT_MSGS 0
#define PRINT_TASKS_RECV_MSGS 0

#define PRINT_ITERATION_TIME 	1
#define PRINT_LB_TIME 				1

#define INT_OPERATIONS 		0
#define FLOAT_OPERATIONS 	1

using namespace std;

enum CommGraph	{
	Ring = 0,
	Mesh2D = 1,
	Mesh3D = 2
};

#include "configHeader.h"

CProxy_Main mainProxy; 			/* readonly */
unsigned long intOpByMs; 		/* readonly */
unsigned long floatOpByMs;	/* readonly */
int numberOfTasks;					/* readonly */
int numberOfIterations; 		/* readonly */

void workByTime(unsigned long time, int operationType);
void workByRepetitions(unsigned long repetitions, int operationType);
unsigned long adjustLoad(int operationType);

class Message: public CMessage_Message {
	public:
	int numberOfBytes;
	char *data;

	public:
	Message(){};
	~Message(){ delete [] data; }
};

class Main: public CBase_Main {
	public:
	CProxy_Task taskArray;

	#if DEBUG
		int *tasksPUs;
		double *tasksWorkTime;
		int *tasksOpType;
		int *tasksLoad;
		int *tasksSize;
		int *tasksMsgsSent;
		int *tasksMsgsReceived;
	#endif

	//control
	int responsesCounter;
	int numLBCalls; //number of LB calls
	int LBFrequency;

	//iteration time
	int currentIteration;
	double iterationInitialTime;
	double *iterationTimes; //store iterations times

	//LB time
	int LBCallIndex;
	double LBCallInitialTime;
	double *LBCallTimes; //store LB times

	Main(CkArgMsg *m){
		if(m->argc != 1) CkExit();

		//set readonly variables
		mainProxy = thisProxy;
		intOpByMs = adjustLoad(INT_OPERATIONS); //get number of integer operations by millisecond
		floatOpByMs = adjustLoad(FLOAT_OPERATIONS); //get floating point operations by millisecond
		numberOfTasks = getNumberOfTasks();
		numberOfIterations = getNumberOfIterations();

		//Get parameters from config file
		LBFrequency = getLBFrequency();

		//print configuration info
		CkPrintf("\nUsing %d processors.\n", 				CkNumPes());
		CkPrintf("Number of tasks: %d\n", 					numberOfTasks);
		CkPrintf("Number of iterations: %d\n", 			numberOfIterations);
		CkPrintf("Communication graph: %s\n",				getExpr("gcomm").c_str());
		CkPrintf("LB call frequency: %s\n", 				getExpr("lbfreq").c_str());
		CkPrintf("Int operations: %s\n", 						getExpr("int_op").c_str());
		CkPrintf("int/ms: %lu\tfloat/ms: %lu\n", 		intOpByMs, floatOpByMs);
		CkPrintf("Tasks sizes: %s\n", 							getExpr("tasksize").c_str());
		CkPrintf("Tasks computational load: %s\n", 	getExpr("load").c_str());
		CkPrintf("Message sizes: %s\n",							getExpr("msgsize").c_str());
		CkPrintf("Number of messages sent: %s\n", 	getExpr("msgnum").c_str());
		CkPrintf("Initial mapping: %s\n", 					getExpr("initmap").c_str());
		CkPrintf("\n");

		//initialize some control variables
		numLBCalls = (numberOfIterations % LBFrequency == 0) ? (numberOfIterations/LBFrequency) - 1 : (numberOfIterations/LBFrequency); //count the number of LB calls
		LBCallIndex = 0;
		LBCallTimes = new double[numLBCalls];
		currentIteration = 1;
		iterationTimes = new double[numberOfIterations];
		responsesCounter = 0;

		#if DEBUG
			tasksPUs = new int[numberOfTasks * numberOfIterations];
			tasksWorkTime = new double[numberOfTasks * numberOfIterations];
			tasksOpType = new int[numberOfTasks * numberOfIterations];
			tasksLoad = new int[numberOfTasks * numberOfIterations];
			tasksSize = new int[numberOfTasks * numberOfIterations];
			tasksMsgsSent = new int[numberOfTasks * numberOfIterations];
			tasksMsgsReceived = new int[numberOfTasks * numberOfIterations];
		#endif

		taskArray = CProxy_Task::ckNew();
		int penum;
		for(int i = 0; i < numberOfTasks; i++){
			penum = getInitialMapping(i, CkNumPes());
			taskArray[i].insert(penum);
		}
		taskArray.doneInserting();

		CkCallback *cb = new CkCallback(CkIndex_Main::endIteration(NULL), thisProxy);
		taskArray.ckSetReductionClient(cb);

		startIteration();
	}

	void endIteration(CkReductionMsg *msg){
		delete msg;
		iterationTimes[currentIteration - 1] = (CkWallTimer() - iterationInitialTime) * 1e3;//store iteration time
		taskArray.getInfo();//get tasks information
	}	

	void receiveInfo(int task, int iteration, int pu, double workTime, int opType, int load, int size, int msgsSent, int msgsReceived){
		//receive info from each task
		#if DEBUG
			tasksPUs[numberOfIterations * task + (iteration - 1)] = pu;
			tasksWorkTime[numberOfIterations * task + (iteration - 1)] = workTime * 1e3;
			tasksOpType[numberOfIterations * task + (iteration - 1)] = opType;
			tasksLoad[numberOfIterations * task + (iteration - 1)] = load;
			tasksSize[numberOfIterations * task + (iteration - 1)] = size;
			tasksMsgsSent[numberOfIterations * task + (iteration - 1)] = msgsSent;
			tasksMsgsReceived[numberOfIterations * task + (iteration - 1)] = msgsReceived;
		#endif

		responsesCounter++;
		if(responsesCounter == numberOfTasks){
			responsesCounter = 0;
			currentIteration++;
			//quit if the last iteration is done
			if(currentIteration > numberOfIterations){
				finish();
				return;
			}
			setUpIteration();
		}
	}

	void setUpIteration(){
		//call LB if it must be called
		if(currentIteration != 1 && ((currentIteration - 1) % LBFrequency == 0)){
			LBCallInitialTime = CkWallTimer();
			taskArray.waitForLB();
			return;
		}
		startIteration();
	}

	void resumeFromLB(){
		//store LB time
		LBCallTimes[LBCallIndex] = (CkWallTimer() - LBCallInitialTime) * 1e3;
		LBCallIndex++;
		startIteration();
	}

	void startIteration(){
		iterationInitialTime = CkWallTimer();
		taskArray.startWorking();
	}

	void finish(){
		#if DEBUG
			CkPrintf("\n");
			#if PRINT_TASKS_PUS
				CkPrintf("1. Tasks' PU in each iteration\n");
				for(int i = 0; i < numberOfTasks; i++){
					CkPrintf("%d\t", i);
					for(int j = 0; j < numberOfIterations; j++)
						CkPrintf("%d\t", tasksPUs[i * numberOfIterations + j]);
					CkPrintf("\n");
				}
				CkPrintf("\n");
			#endif

			#if PRINT_TASKS_WORK_TIME
				CkPrintf("2. Tasks' work time in each iteration\n");
				for(int i = 0; i < numberOfTasks; i++){
					CkPrintf("%d\t", i);
					for(int j = 0; j < numberOfIterations; j++)
						CkPrintf("%.1f\t", tasksWorkTime[i * numberOfIterations + j]);
					CkPrintf("\n");
				}
				CkPrintf("\n");
			#endif

			#if PRINT_TASKS_OPTYPE
				CkPrintf("3. Tasks' operation type in each iteration\n");
				for(int i = 0; i < numberOfTasks; i++){
					CkPrintf("%d\t", i);
					for(int j = 0; j < numberOfIterations; j++)
						CkPrintf("%c\t", (tasksOpType[i * numberOfIterations + j] == 1 ? 'f' : 'i'));
					CkPrintf("\n");
				}
				CkPrintf("\n");
			#endif

			#if PRINT_TASKS_LOAD
				CkPrintf("4. Tasks' load in each iteration\n");
				for(int i = 0; i < numberOfTasks; i++){
					CkPrintf("%d\t", i);
					for(int j = 0; j < numberOfIterations; j++)
						CkPrintf("%d\t", tasksLoad[i * numberOfIterations + j]);
					CkPrintf("\n");
				}
				CkPrintf("\n");
			#endif

			#if PRINT_TASKS_SIZE
				CkPrintf("5. Tasks' sizes\n");
				for(int i = 0; i < numberOfTasks; i++){
					CkPrintf("%d\t", i);
					for(int j = 0; j < numberOfIterations; j++)
						CkPrintf("%d\t", tasksSize[i * numberOfIterations + j]);
					CkPrintf("\n");
				}
				CkPrintf("\n");
			#endif

			#if PRINT_TASKS_SENT_MSGS
				CkPrintf("6. Tasks' num messages sent in each iteration\n");
				for(int i = 0; i < numberOfTasks; i++){
					CkPrintf("%d\t", i);
					for(int j = 0; j < numberOfIterations; j++)
						CkPrintf("%d\t", tasksMsgsSent[i * numberOfIterations + j]);
					CkPrintf("\n");
				}
				CkPrintf("\n");
			#endif

			#if PRINT_TASKS_RECV_MSGS
				CkPrintf("7. Tasks' num messages received in each iteration\n");
				for(int i = 0; i < numberOfTasks; i++){
					CkPrintf("%d\t", i);
					for(int j = 0; j < numberOfIterations; j++)
						CkPrintf("%d\t", tasksMsgsReceived[i * numberOfIterations + j]);
					CkPrintf("\n");
				}
				CkPrintf("\n");
			#endif
		#endif

		#if PRINT_ITERATION_TIME
			CkPrintf("Iteration times "); CkPrintf("%.1f ", iterationTimes[0]); for(int i = 1; i < numberOfIterations; i++) CkPrintf(",%.1f ", iterationTimes[i]);
			double iterationTimeSum = 0; for(int i = 0; i < numberOfIterations; i++) iterationTimeSum += iterationTimes[i];
			CkPrintf("\nIteration total time %.1f\n", iterationTimeSum);
		#endif

		#if PRINT_LB_TIME
			if(numLBCalls > 0){
				CkPrintf("LB times "); for(int i = 0; i < numLBCalls; i++) CkPrintf("%.1f ", LBCallTimes[i]);
				double LBTimeSum = 0; for(int i = 0; i < numLBCalls; i++) LBTimeSum += LBCallTimes[i];
				CkPrintf("\nLB total time %.1f\n", LBTimeSum);
			}
		#endif

		CkExit();//end program
	}
};

class Task : public CBase_Task {
	public:

	//neighbors info
	int commGraphType;
	int numSenders; //tasks that can send msgs to this task
	int *sendersIndex; //senders' index
	int numReceivers; //tasks that can receive msgs from this task
	int *receiversIndex; //receivers' index
	int *indexInSendersReceiversList;
	
	//time info
	double initialWorkTime;
	double workTime;

	//attributes
	int taskSize; //number of bytes allocated for the task
	int operationType; //type of operation executed on work function
	int currentLoad; //current computational load

	//control
	bool msgsSent;
	int msgCounter;
	int incomingMessages;
	int currentIteration;
	char *data;
	int msgSentCounter;

	Task(){
		msgsSent = false;
		msgCounter = 0;
		incomingMessages = 0;
		currentIteration = 1;
		msgSentCounter = 0;
		usesAtSync = CmiTrue;

		//set up attributes
		commGraphType = getCommunicationGraph();
		switch(commGraphType){
			case Ring: createRing(thisIndex); break;
			case Mesh2D: create2DMesh(thisIndex); break;
			case Mesh3D: create3DMesh(thisIndex); break;
		}
		taskSize = getTaskSize(thisIndex);
		data = new char[taskSize];
		operationType = (getIntOper(thisIndex, currentIteration) ? INT_OPERATIONS : FLOAT_OPERATIONS);
		currentLoad = getLoad(thisIndex, currentIteration);

		//get task incoming msgs number in the first iteration
		for(int i = 0; i < numSenders; i++){
				incomingMessages += getMessageNumber(sendersIndex[i], currentIteration, indexInSendersReceiversList[i]);
		}
	}

	~Task(){
		delete [] sendersIndex;
		delete [] indexInSendersReceiversList;
		delete [] receiversIndex;
		delete [] data;
	}

	Task(CkMigrateMessage *m){}

	void pup(PUP::er &p){
		CBase_Task::pup(p);
		p(numSenders);
		p(numReceivers);

		if(p.isUnpacking())
			receiversIndex = new int[numReceivers];
		PUParray(p, receiversIndex, numReceivers);
		if(p.isUnpacking())
			sendersIndex = new int[numSenders];
		PUParray(p, sendersIndex, numSenders);

		p(commGraphType);
		p(taskSize);
		p(operationType);
		p(currentLoad);

		p(msgsSent);
		p(msgCounter);
		p(incomingMessages);
		p(currentIteration);

		if(p.isUnpacking())
			data = new char[taskSize];
		PUParray(p, data, taskSize);

		if(p.isUnpacking())
			indexInSendersReceiversList = new int[numSenders];
		PUParray(p, indexInSendersReceiversList, numSenders);
	}

	void waitForLB(){
		AtSync();
	}

	void ResumeFromSync(){//Called by load-balancing framework
		CkCallback cb(CkIndex_Main::resumeFromLB(), mainProxy);
		contribute(0, NULL, CkReduction::sum_int, cb);
	}

	void createRing(int index){
		//only 1 sender and 1 receiver for each task
		numSenders = 1;
		numReceivers = 1;
		receiversIndex = new int[1];
		sendersIndex = new int[1];

		//get receiver and senders indexes
		receiversIndex[0] = ((index+1 == numberOfTasks) ? 0 : index + 1);
		sendersIndex[0] = ((index-1 < 0) ? numberOfTasks - 1 : index - 1);

		//get task index on senders' receiver list
		indexInSendersReceiversList = new int[1];
		indexInSendersReceiversList[0] = 0;
	}

	void create2DMesh(int index){
		//max of 4 senders and 4 receivers for each task
		numSenders = 0;
		numReceivers = 0;
		receiversIndex = new int[4];
		sendersIndex = new int[4];

		//get number of lines and cols of the 2D mesh
		int lines = ceil(sqrt(numberOfTasks));
		int cols = ceil((double)numberOfTasks / (double)lines);

		//find task position
		int taskLine = index / cols;
		int taskCol = index % cols;

		//create possible neighbors list
		int neighborsLines[4] = {taskLine-1, taskLine, taskLine+1, taskLine};
		int neighborsCols[4] = {taskCol, taskCol+1, taskCol, taskCol-1};

		//get receiver and senders indexes
		for(int possibleNeighbor = 0; possibleNeighbor < 4; possibleNeighbor++){
			int neighborLine = neighborsLines[possibleNeighbor];
			int neighborCol = neighborsCols[possibleNeighbor];

			//check if it is a valid neighbor position
			if(neighborLine >= lines || neighborLine < 0 || neighborCol >= cols || neighborCol < 0) continue;
			int neighborIndex = (neighborLine * cols) + neighborCol;
			if(neighborIndex >= numberOfTasks || neighborIndex < 0) continue;

			receiversIndex[numReceivers] = neighborIndex;
			sendersIndex[numSenders] = neighborIndex;

			numReceivers++;
			numSenders++;
		}

		//get task index on senders' receiver list
		indexInSendersReceiversList = new int[4];

		//for each sender
		for(int sender = 0; sender < numSenders; sender++){
			//get sender line and col
			int senderIndex = sendersIndex[sender];
			int senderLine = senderIndex / cols;
			int senderCol = senderIndex % cols;

			//get possible neighbors from sender
			int receiversLines[4] = {senderLine-1, senderLine, senderLine+1, senderLine};
			int receiversCols[4] = {senderCol, senderCol+1, senderCol, senderCol-1};

			int receiverCounter = 0;

			//get possible receiver line and col
			for(int possibleNeighbor = 0; possibleNeighbor < 4; possibleNeighbor++){
				int receiverLine = receiversLines[possibleNeighbor];
				int receiverCol = receiversCols[possibleNeighbor];

				//check if it is a valid receiver position
				if(receiverLine >= lines || receiverLine < 0 || receiverCol >= cols || receiverCol < 0) continue;
				int receiverIndex = (receiverLine * cols) + receiverCol;
				if(receiverIndex >= numberOfTasks || receiverIndex < 0) continue;

				//if receiver position is task position, then 'receiverIndex' is the task's index in the sender's receiver list
				if(receiverLine == taskLine && receiverCol == taskCol){
					indexInSendersReceiversList[sender] = receiverCounter;
					break;
				}
				else receiverCounter++;
			}
		}
	}

	void create3DMesh(int index){
		//max of 6 senders and 6 receivers for each task
		numSenders = 0;
		numReceivers = 0;
		receiversIndex = new int[6];
		sendersIndex = new int[6];

		//get dimensions sizes of the 3D mesh
		int dim = (int)ceil(pow((double)numberOfTasks,(double)1/3));

		//find task position
		int xTask = index % dim;
		int yTask = ((index - xTask) % (dim*dim)) / dim;
		int zTask = (index - yTask*dim -xTask)/(dim*dim);

		//create possible neighbors list
		int xNeighbors[6] = {xTask-1, xTask+1, xTask, xTask, xTask, xTask};
		int yNeighbors[6] = {yTask, yTask, yTask-1, yTask+1, yTask, yTask};
		int zNeighbors[6] = {zTask, zTask, zTask, zTask, zTask-1, zTask+1};

		//get receiver and senders indexes
		for(int possibleNeighbor = 0; possibleNeighbor < 6; possibleNeighbor++){
			int xNeighbor = xNeighbors[possibleNeighbor];
			int yNeighbor = yNeighbors[possibleNeighbor];
			int zNeighbor = zNeighbors[possibleNeighbor];

			//check if it is a valid neighbor position
			if(xNeighbor >= dim || xNeighbor < 0 || yNeighbor >= dim || yNeighbor < 0 || zNeighbor >= dim || zNeighbor < 0 ) continue;
			int neighborIndex = zNeighbor * dim*dim + yNeighbor*dim + xNeighbor;
			if(neighborIndex >= numberOfTasks || neighborIndex < 0)  continue;

			receiversIndex[numReceivers] = neighborIndex;
			sendersIndex[numSenders] = neighborIndex;

			numSenders++;
			numReceivers++;
		}

		//get task index on senders' receiver list
		indexInSendersReceiversList = new int[6];

		//for each sender
		for(int sender = 0; sender < numSenders; sender++){
			//get sender position
			int senderIndex = sendersIndex[sender];
			int xSender = senderIndex % dim;
			int ySender = ((senderIndex - xSender) % (dim*dim)) / dim;
			int zSender = (senderIndex - ySender*dim -xSender)/(dim*dim);

			//get possible neighbors from sender
			int xReceivers[6] = {xSender-1, xSender+1, xSender, xSender, xSender, xSender};
			int yReceivers[6] = {ySender, ySender, ySender-1, ySender+1, ySender, ySender};
			int zReceivers[6] = {zSender, zSender, zSender, zSender, zSender-1, zSender+1};

			int receiverCounter = 0;

			//get possible receiver positions
			for(int possibleNeighbor = 0; possibleNeighbor < 6; possibleNeighbor++){
				int xReceiver = xReceivers[possibleNeighbor];
				int yReceiver = yReceivers[possibleNeighbor];
				int zReceiver = zReceivers[possibleNeighbor];

				//check if it is a valid receiver position
				if(xReceiver >= dim || xReceiver < 0 || yReceiver >= dim || yReceiver < 0 || zReceiver >= dim || zReceiver < 0 ) continue;
				int neighborIndex = zReceiver * dim*dim + yReceiver*dim + xReceiver;
				if(neighborIndex >= numberOfTasks || neighborIndex < 0)  continue;

				//if receiver position is task position, then 'receiverIndex' is the task's index in the sender's receiver list
				if(xReceiver == xTask && yReceiver == yTask && zReceiver == zTask){
					indexInSendersReceiversList[sender] = receiverCounter;
					break;
				}
				else receiverCounter++;
			}
		}
	}

	/* Work, send messages to neighbors and wait for neighbors' messages */
	void startWorking(){
		//work
		initialWorkTime = CkWallTimer();
		workByTime(currentLoad, operationType);
		workTime = CkWallTimer() - initialWorkTime;

		//communicate with neighbors
		int msgNumber;
		for(int i = 0; i < numReceivers; i++){ //for each task's receivers
			msgNumber = getMessageNumber(thisIndex, currentIteration, i);
			//CkPrintf("[%d] sending %d messages to neighbor %d\n", thisIndex, msgNumber, receiversIndex[i]);
			for(int j = 0; j < msgNumber; j++, msgSentCounter++){ //send n messages
				Message *msg = new Message;
				msg->numberOfBytes = getMessageSize(thisIndex, currentIteration, i, j);
				msg->data = new char[msg->numberOfBytes];
				thisProxy(receiversIndex[i]).receiveMessage(msg); //send message
			}
		}
		msgsSent = true;

		
		//if all neighbors' messages have been received, end iteration
		if(msgCounter == incomingMessages){
			contribute(sizeof(double), &workTime, CkReduction::max_double);
		}
		//otherwise, wait for neighbors' messages...
	}

	/* Receive a message from a neighbor */
	void receiveMessage(Message *msg){
		msgCounter++;
		//CkPrintf("[%d] received a message! [%d/%d]\n", thisIndex, msgCounter, incomingMessages);

		//if all messages have been received, end iteration
		if(msgCounter == incomingMessages && msgsSent){
			contribute(sizeof(double), &workTime, CkReduction::max_double);
		}
	}

	/* Update attributes for next iteration */
	void updateAttributes(){
		currentIteration++;
		msgsSent = false;
		msgCounter = 0;
		incomingMessages = 0;
		msgSentCounter = 0;
		for(int i = 0; i < numSenders; i++){ //for each task's sender
				incomingMessages += getMessageNumber(sendersIndex[i], currentIteration, indexInSendersReceiversList[i]);
		}
		operationType = (getIntOper(thisIndex, currentIteration) ? INT_OPERATIONS : FLOAT_OPERATIONS);
		currentLoad = getLoad(thisIndex, currentIteration);
	}

	/* Send some info to the main chare / update attributes */
	void getInfo(){
		//send info
		mainProxy.receiveInfo(thisIndex, currentIteration, CkMyPe(), workTime, operationType, currentLoad, taskSize, msgSentCounter, incomingMessages);
		updateAttributes();
	}

};

void workByTime(unsigned long time, int operationType){
	unsigned long i, repetitions;
	int result = 0;
	double endTime = CmiWallTimer() + ((double)time/1000);
	switch(operationType){
		case INT_OPERATIONS:
			while(CmiWallTimer() < endTime){
				for(int repeticao = 1; repeticao < intOpByMs; repeticao++){
					result = ((((result+7)*23)/(((result/7)+47)*result)) * ((((((result+7)*23)/result)/7)+47)*result)) / (((result+7)*23)/(((result/7)+47)*result))*7;
				}
			}
			break;
		case FLOAT_OPERATIONS:
			while(CmiWallTimer() < endTime){
				for(int repeticao = 1; repeticao < floatOpByMs; repeticao++){
					result = (int)(sqrt(1+cos(result*1.57)));
				}
			}
			break;
	}
}

void workByRepetitions(unsigned long repetitions, int operationType){
	unsigned long i;
	int result = 0;
	switch(operationType){
		case INT_OPERATIONS:
			for(i = 1; i <= repetitions; i++){
				result = ((((result+7)*23)/(((result/7)+47)*result)) * ((((((result+7)*23)/result)/7)+47)*result)) / (((result+7)*23)/(((result/7)+47)*result))*7;
			}
			break;
		case FLOAT_OPERATIONS:
			for(i = 1; i <= repetitions; i++){
				result = (int)(sqrt(1+cos(result*1.57)));
			}
			break;
	}
}

unsigned long adjustLoad(int operationType){
	//estimate the number of repetitions in 0.05 seconds
	unsigned long repetitions = 0;
	double adjustTime = 0.05;//0.05 seconds
	double endTime = CmiWallTimer() + adjustTime;

	while(CmiWallTimer() < endTime){
		workByRepetitions(5, operationType);
		repetitions += 5;
	}

	//execute some correction cycles
	int correctionCycles = 5;
	double initialTime;
	double correction;
	for(int i = 0; i < correctionCycles; i++){
		initialTime = CmiWallTimer();
		workByRepetitions(repetitions, operationType);
		endTime = CmiWallTimer();

		correction = adjustTime/(endTime - initialTime);
		repetitions *= correction;
	}

	//return the number of repetitions in 1 millisecond
	unsigned long repetitionsBySecond = (unsigned long)(repetitions/adjustTime);
	return repetitionsBySecond * 1e-3;
};

#include "teste.def.h"
