#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>

using namespace std;

void trim(string &exp){
	string chars = " \t";
	int pos = exp.find_last_not_of(chars);
	if(pos != string::npos){
		exp.erase(pos + 1);
		pos = exp.find_first_not_of(chars);
		if(pos != string::npos) exp.erase(0, pos);
	}
	else exp.erase(exp.begin(), exp.end());
}

void replaceAll(string& str, const string& from, const string& to) {
    if(from.empty()) return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

int main(int argc, char **argv){
	if(argc != 2) return 1;
	string fileName = argv[1];

	//open config file and get parameters' expressions
	string taskNumberExp, iterNumberExp, gCommExp, lbFreqExp, intOperExp, taskSizeExp, loadExp, msgSizeExp, msgNumExp, initMapExp;
	string line;
	fstream configFile;
	configFile.open(fileName.c_str());
	while(!configFile.eof()){
		getline(configFile, line);
		if(line.length() == 0) continue; //skip empty line
		else if(line[0] == '#') continue; //skip comment

		else if(line[0] == 'n') //read number of tasks
			taskNumberExp = line.erase(0, line.find('=') + 1);
		else if(line[0] == 'r') //read number of iterations
			iterNumberExp = line.erase(0, line.find('=') + 1);
		else if(line.substr(0,5) == "gcomm") //read communication graph
			gCommExp = line.erase(0, line.find('=') + 1);
		else if(line.substr(0,6) == "lbfreq") //read lb frequency expression
			lbFreqExp = line.erase(0, line.find('=') + 1);
		else if(line.substr(0,6) == "int_op") //read int operations expression
			intOperExp = line.erase(0, line.find('=') + 1);
		else if(line.substr(0,8) == "tasksize") //read task size expression
			taskSizeExp = line.erase(0, line.find('=') + 1);
		else if(line.substr(0,4) == "load") //read load expression
			loadExp = line.erase(0, line.find('=') + 1);
		else if(line.substr(0,7) == "msgsize") //read message size expression
			msgSizeExp = line.erase(0, line.find('=') + 1);
		else if(line.substr(0,6) == "msgnum") //read message number expression
			msgNumExp = line.erase(0, line.find('=') + 1);
		else if(line.substr(0,7) == "initmap") //read initial mapping expression
			initMapExp = line.erase(0, line.find('=') + 1);
		else{
			cout << "error while reading line:\n" << line << endl; break;
		}
	}
	configFile.close();

	//remove extra white spaces of an expression
	trim(taskNumberExp);
	trim(iterNumberExp);
	trim(gCommExp);
	trim(lbFreqExp);
	trim(intOperExp);
	trim(taskSizeExp);
	trim(loadExp);
	trim(msgSizeExp);
	trim(msgNumExp);
	trim(initMapExp);

	cout << "#include <cmath>" << endl;
	cout << "#include <string>" << endl;
	cout << "using namespace std;" << endl;

	//print functions
	cout << "int getNumberOfTasks(){" << endl;
		cout << "\treturn " << taskNumberExp << ";" << endl;
	cout << "}" << endl;

	cout << "int getNumberOfIterations(){" << endl;
		cout << "\treturn " << iterNumberExp << ";" << endl;
	cout << "}" << endl;

	cout << "int getLBFrequency(){" << endl;
		cout << "\treturn " << lbFreqExp << ";" << endl;
	cout << "}" << endl;

	cout << "int getInitialMapping(int task, int numPes){" << endl;
		cout << "\tint n = " << taskNumberExp << ";" << endl;
		cout << "\tint t = task;" << endl;
		cout << "\tint p = numPes;" << endl;
		cout << "\treturn " << initMapExp << ";" << endl;
	cout << "}" << endl;

	cout << "int getCommunicationGraph(){" << endl;
		cout << "\treturn (int)" << gCommExp << ";" << endl;
	cout << "}" << endl;

	cout << "bool getIntOper(int task, int iteration){" << endl;
		cout << "\tint n = " << taskNumberExp << ";" << endl;
		cout << "\tint r = " << iterNumberExp << ";" << endl;
		cout << "\tint t = task;" << endl;
		cout << "\tint i = iteration;" << endl;
		cout << "\treturn " << intOperExp << ";" << endl;
	cout << "}" << endl;

	cout << "int getTaskSize(int task){" << endl;
		cout << "\tint n = " << taskNumberExp << ";" << endl;
		cout << "\tint r = " << iterNumberExp << ";" << endl;
		cout << "\tint t = task;" << endl;
		cout << "\treturn " << taskSizeExp << ";" << endl;
	cout << "}" << endl;

	cout << "int getMessageSize(int task, int iteration, int neighbor, int msgIndex){" << endl;
		cout << "\tint n = " << taskNumberExp << ";" << endl;
		cout << "\tint r = " << iterNumberExp << ";" << endl;
		cout << "\tint t = task;" << endl;
		cout << "\tint i = iteration;" << endl;
		cout << "\tint v = neighbor;" << endl;
		cout << "\tint u = msgIndex;" << endl;
		cout << "\treturn " << msgSizeExp << ";" << endl;
	cout << "}" << endl;

	cout << "int getMessageNumber(int task, int iteration, int neighbor){" << endl;
		cout << "\tint n = " << taskNumberExp << ";" << endl;
		cout << "\tint r = " << iterNumberExp << ";" << endl;
		cout << "\tint t = task;" << endl;
		cout << "\tint i = iteration;" << endl;
		cout << "\tint v = neighbor;" << endl;
		cout << "\treturn " << msgNumExp << ";" << endl;
	cout << "}" << endl;

	cout << "int getLoad(int task, int iteration){" << endl;
		cout << "\tint n = " << taskNumberExp << ";" << endl;
		cout << "\tint r = " << iterNumberExp << ";" << endl;
		cout << "\tint t = task;" << endl;
		cout << "\tint i = iteration;" << endl;
		replaceAll(loadExp, "$", "getLoad(t,i-1)");
		cout << "\treturn " << loadExp << ";" << endl;
	cout << "}" << endl;

	//print info function
	cout << "string getExpr(string attribute){" << endl;
		cout << "\tif(attribute == \"n\")" << endl;
			cout << "\t\treturn \"" << taskNumberExp << "\";" << endl;
		cout << "\telse if(attribute == \"r\")" << endl;
			cout << "\t\treturn \"" << iterNumberExp << "\";" << endl;
		cout << "\telse if(attribute == \"gcomm\")" << endl;
			cout << "\t\treturn \"" << gCommExp << "\";" << endl;
		cout << "\telse if(attribute == \"lbfreq\")" << endl;
			cout << "\t\treturn \"" << lbFreqExp << "\";" << endl;
		cout << "\telse if(attribute == \"int_op\")" << endl;
			cout << "\t\treturn \"" << intOperExp << "\";" << endl;
		cout << "\telse if(attribute == \"tasksize\")" << endl;
			cout << "\t\treturn \"" << taskSizeExp << "\";" << endl;
		cout << "\telse if(attribute == \"load\")" << endl;
			cout << "\t\treturn \"" << loadExp << "\";" << endl;
		cout << "\telse if(attribute == \"msgsize\")" << endl;
			cout << "\t\treturn \"" << msgSizeExp << "\";" << endl;
		cout << "\telse if(attribute == \"msgnum\")" << endl;
			cout << "\t\treturn \"" << msgNumExp << "\";" << endl;
		cout << "\telse if(attribute == \"initmap\")" << endl;
			cout << "\t\treturn \"" << initMapExp << "\";" << endl;
		cout << "\treturn \"\";" << endl;
	cout << "}" << endl;

	return 0;
}
















