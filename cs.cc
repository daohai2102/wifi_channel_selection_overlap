#include <iostream>
#include <cstdio>
#include <fstream>
#include <unordered_map>
#include <list>
#include <vector>
#include <random>
#include <sstream>
#include "SshSession.h"

using namespace std;


#define MAX_UTIL 0.95
#define LIMIT_UTIL 0.9			//use to make decision
#define MIN_ENV_UTIL 0.1
#define MIN_TOTAL_UTIL 0.3
#define DELTA_UTIL 0.1
#define LIMIT_ENV_UTIL 0.6		//need to make experiment to determine the optimze value

const string topoFile = "topo.txt";
const string apCredentialFile = "ap_credential.csv";
const string statFilename = "statistic.csv";

struct ChannelUtilization{
	float totalUtil;
	float envUtil;
};

typedef unordered_map<int, ChannelUtilization> Domain;

struct AccessPoint{
	int id;
	int channel;
	SshSession sshAp;
	Domain domain;
	vector<AccessPoint*> adjacentAps;
	void populateChannelUtilization();
	void switchChannel(int chan);
};

class ChannelSwitching{
private:
	unsigned int nAP;
	vector<AccessPoint*> apList;
	bool *assignedAps;
	ofstream statFile;
public:
	ChannelSwitching();
	~ChannelSwitching();
	void readTopoFromFile(const string filename);
	void readSshInfo(const string filename);
	void prepareData();
	void showChannel();
	void showDomain();
	void showTopo();
	void assignChannel();
	void assignChannelRecursively(AccessPoint *ap);
};

int getMinUtilChannel(Domain);
int getOptimalChannel(unordered_map<int,bool>, Domain domain);

int main(){
	ChannelSwitching cs;
	cs.showTopo();
	cs.showDomain();
	cs.assignChannel();
	cs.showChannel();
	return 0;
}

ChannelSwitching::ChannelSwitching(){
	cout << "Entering ChannelSwitching::ChannelSwitching()\n";
	prepareData();
	
	bool fileExisted = false;
	if (FILE *file = fopen(statFilename.c_str(), "r")){
		fclose(file);
		fileExisted = true;
	}

	statFile.open(statFilename, ofstream::app);
	if (!statFile.is_open()){
		cerr << "Cannot open statistic file\n";
		exit(1);
	}

	if (!fileExisted){
		statFile << "ap_id,old_channel,old_avail,old_env,old_total,new_channel,new_avail,new_env,new_total\n";
	}
	cout << "Leaving ChannelSwitching::ChannelSwitching()\n";
}

ChannelSwitching::~ChannelSwitching(){
	cout << "Entering ChannelSwitching::~ChannelSwitching()\n";
	delete[] assignedAps;
	for (unsigned int i = 0; i < nAP; i++){
		delete apList[i];
	}
	delete[] assignedAps;
	statFile.close();
	cout << "Leaving ChannelSwitching::~ChannelSwitching()\n";
}

void ChannelSwitching::readTopoFromFile(const string filename){
	cout << "Entering ChannelSwitching::readTopoFromFile()\n";

	ifstream myfile(filename);
	if (myfile.is_open()){
		myfile >> nAP;

		assignedAps = new bool[nAP];
		
		for (unsigned int i = 0; i < nAP; i++){
			AccessPoint *ap = new AccessPoint();
			ap->channel = 0;
			ap->id = i;
			apList.push_back(ap);
			assignedAps[i] = false;
		}

		for (unsigned int i = 0; i < nAP; ++i){
			unsigned int nAdj;
			myfile >> nAdj;
			for (unsigned int j = 0; j < nAdj; j++){
				int apIndex;
				myfile >> apIndex;
				apList[i]->adjacentAps.push_back(apList[apIndex]);
			}
		}
	}

	cout << "Leaving ChannelSwitching::readTopoFromFile()\n";
}

void ChannelSwitching::prepareData(){
	cout << "Entering ChannelSwitching::prepareData()\n";
	//srand(time(NULL));
	readTopoFromFile(topoFile);
	readSshInfo(apCredentialFile);
	
	for (unsigned int i = 0; i < nAP; i++){
		apList[i]->sshAp.connectSsh();
	}

	//default_random_engine totalGenerator;
	//uniform_real_distribution<double> totalDistribution(MIN_TOTAL_UTIL, MAX_UTIL);
	//for (unsigned int i = 0; i < nAP; i++){
	//	for (unsigned int j = 1; j < 12; j+=5){
	//		apList[i]->domain.insert(pair<int,ChannelUtilization>(j, ChannelUtilization()));

	//		/* generate channel utilization randomly
	//		 * total_util:	[0.3, MAX_UTIL] => distribution: uniform
	//		 * env_util:	[0.1, total_util] => distribution: uniform*/
	//		float totalUtil = totalDistribution(totalGenerator);
	//		apList[i]->domain[j].totalUtil = totalUtil;
	//		apList[i]->domain[j].envUtil = MIN_ENV_UTIL + rand()/((float)RAND_MAX/(totalUtil - MIN_ENV_UTIL));
	//	}
	//}

	cout << "Leaving ChannelSwitching::prepareData()\n";
}

void ChannelSwitching::showChannel(){
	cout << "Entering ChannelSwitching::showChannel()\n";

	for (unsigned int i = 0; i < nAP; i++){
		cout << "AP " << apList[i]->id << ": " << apList[i]->channel << '\n';
	}

	cout << "Leaving ChannelSwitching::showChannel()\n";
}

void ChannelSwitching::showDomain(){
	cout << "Entering ChannelSwitching::showDomain()\n";

	for (unsigned int i = 0; i < nAP; i++){
		cout << "AP " << apList[i]->id << ": ";
		for (unsigned int j = 1; j < 12; j+=5){
			ChannelUtilization *util = &(apList[i]->domain[j]);
			cout << "(" << j << "," << util->envUtil << "," << util->totalUtil << ")\t";
		}
		cout << '\n';
	}

	cout << "Leaving ChannelSwitching::showDomain()\n";
}

void ChannelSwitching::showTopo(){
	cout << "Entering ChannelSwitching::showTopo()\n";

	for (unsigned int i = 0; i < nAP; i++){
		cout << "AP " << apList[i]->id << ": ";
		vector<AccessPoint*> *adj = &(apList[i]->adjacentAps);
		for (auto it = adj->begin(); it != adj->end(); ++it){
			cout << (*it)->id << ' ';
		}
		cout << '\n';
	}

	cout << "Leaving ChannelSwitching::showTopo()\n";
}

void ChannelSwitching::assignChannelRecursively(AccessPoint *ap){
	cout << "Entering assignChannelRecursively() with AP " << ap->id << '\n';

	/* TODO: gathering channel utilization via ssh */
	ap->populateChannelUtilization();

	int oldChannel = ap->channel;


	int channel = getMinUtilChannel(ap->domain);

	cout << "minUtilChannel: (" 
		 << channel << ',' 
		 << ap->domain[channel].envUtil << ',' 
		 << ap->domain[channel].totalUtil << ")\n";

	/* build a list of channels not assigned to adjacent AP */
	unordered_map<int, bool> assignedChannels;
	assignedChannels.insert(pair<int,bool>(1, false));
	assignedChannels.insert(pair<int,bool>(6, false));
	assignedChannels.insert(pair<int,bool>(11, false));

	for (auto it = ap->adjacentAps.begin(); it != ap->adjacentAps.end(); ++it){
		if (assignedAps[(*it)->id]){
			assignedChannels[(*it)->channel] = true;
		}
	}

	int opChannel;
	
	if (ap->domain[channel].totalUtil < LIMIT_UTIL ||
		!assignedChannels[channel]){
		opChannel = channel;
	} else {
		opChannel = getOptimalChannel(assignedChannels, ap->domain);
		if (!opChannel){
			cout << "optimal channel not found => optimalChannel = minUtilChannel\n";
			opChannel = channel;
		}else{
			cout << "optimalChannel: (" 
				 << opChannel << ',' 
				 << ap->domain[opChannel].envUtil << ',' 
				 << ap->domain[opChannel].totalUtil << ")\n";
		}
	}

	ap->switchChannel(opChannel);

	assignedAps[ap->id] = true;

	cout << "assigned channel " << "(" 
		 << opChannel << ','
		 << ap->domain[opChannel].envUtil << ',' 
		 << ap->domain[opChannel].totalUtil << ")"
		 << " to AP " << ap->id << '\n';

	/* export to statistic file */
	float oldEnv = ap->domain[oldChannel].envUtil;
	float oldTotal = ap->domain[oldChannel].totalUtil;
	float oldAvail = LIMIT_UTIL - oldEnv;

	float newEnv = ap->domain[opChannel].envUtil;
	float newTotal = ap->domain[opChannel].totalUtil;
	float newAvail = LIMIT_UTIL - newEnv;

	statFile << ap->id << ','
	   << oldChannel << ',' << oldAvail << ',' << oldEnv << ',' << oldTotal << ','
	   << opChannel << ',' << newAvail << ',' << newEnv << ',' << newTotal << '\n';
	
	/* recursively assign channel to the adjacent APs if those APs have not been assigned*/
	for (auto it = ap->adjacentAps.begin(); it != ap->adjacentAps.end(); ++it){
		if (!assignedAps[(*it)->id]){
			assignChannelRecursively((*it));
		}
	}

	cout << "Leaving assignChannelRecursively() with AP " << ap->id << '\n';
}

void ChannelSwitching::assignChannel(){
	cout << "Entering assignChannel()\n";

	for (unsigned int i = 0; i < nAP; i++){
		if (!assignedAps[i]){
			assignChannelRecursively(apList[0]);
		}
	}

	cout << "Leaving assignChannel()\n";
}

void ChannelSwitching::readSshInfo(const string filename){
	cout << "Entering ChannelSwitching::readSshInfo()\n";

	ifstream myfile(filename);
	if (myfile.is_open()){
		for (unsigned int i = 0; i < nAP; i++){
			string line;
			getline(myfile, line);
			stringstream ss(line);
			string userName, address, portstr, keyfile;
			getline(ss, userName, ',');
			getline(ss, address, ',');
			getline(ss, portstr, ',');
			getline(ss, keyfile, ',');
			int port = stoi(portstr);
			(apList[i]->sshAp).setUserName(userName.c_str())
							  .setAddress(address.c_str())
							  .setPort(port)
							  .setPrivateKeyFile(keyfile.c_str());
		}

	}

	cout << "Leaving ChannelSwitching::readSshInfo()\n";

}

int getMinUtilChannel(Domain domain){
	float minUtil = 1.0;
	int minChannel;
	for (int i = 1; i < 12; i+=5){
		if (domain[i].envUtil < minUtil){
			minUtil = domain[i].envUtil;
			minChannel = i;
		}
	}

	return minChannel;
}

int getOptimalChannel(unordered_map<int,bool> assignedChannels, Domain domain){
	float minUtil = 1.0;
	int minChannel = 0;
	for (int i = 1; i < 12; i++){
		if (!assignedChannels[i] && domain[i].envUtil < LIMIT_ENV_UTIL && domain[i].envUtil < minUtil){
			minUtil = domain[i].envUtil;
			minChannel = i;
		}
	}
	return minChannel;
}

void AccessPoint::populateChannelUtilization(){
	string result = sshAp.runCommand("/home/root/get_chan_util");
	stringstream ss(result);
	ss >> channel;
	for (int i = 1; i < 12; i+=5){
		domain.insert(pair<int,ChannelUtilization>(i, ChannelUtilization()));
		float env, total;
		ss >> env >> total;
		domain[i].totalUtil = total;
		domain[i].envUtil = env;
		
	}
}

void AccessPoint::switchChannel(int chan){
	stringstream ss;
	ss << "/home/root/switch_channel " << chan;
	sshAp.runCommand(ss.str().c_str());
	channel = chan;
}
