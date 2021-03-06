#ifndef FUNCTIONS
#define FUNCTIONS
#include <string>
#include <initializer_list>
#include <iostream>
#include "Download.h"
using std::string; using std::cin; using std::cout;
//the TI is the expected type returned from the function.
template<typename TI, typename F = std::function<bool(TI&)> >
TI check(const string& error_message, const string& message, F fx = std::function<bool(TI&)>([](TI&)->bool {return true; })) {
	//checking inputs recieved, if the stream is good and if they match what characters were wanted.
	TI x;
	while (cin.good()) {
		cin >> x;
		if (cin.bad()) {
			if (!error_message.empty())
				cout << error_message << endl;
			cin.ignore();
			cin.clear();
			continue;
		}
		else {//cin is in a good condition
			bool good = fx(x);
			if (good) {
				return x;
			}
			else {
				if (!message.empty())
					cout << message << endl;
				continue;
			}
		}

	}//while
	return TI();// return empty object if function fails
}

template<typename TI, typename F = std::function<bool(TI&)> >
TI checkGetLine(const string& error_message, const string& message, F fx = std::function<bool(TI&)>([](TI&)->bool {return true; })) {
	//checking inputs recieved, if the stream is good and if they match what characters were wanted with getline.
	TI x;
	while (cin.good()) {
		getline(cin, x);
		if (cin.bad()) {
			if (!error_message.empty())
				cout << error_message << endl;
			cin.ignore();
			cin.clear();
			continue;
		}
		else {//cin is in a good condition
			bool good = fx(x);
			if (good) {
				return x;
			}
			else {
				if (!message.empty())
					cout << message << endl;
				continue;
			}
		}

	}//while
	return TI(); // return empty object if function fails
}

extern size_t writeJsonData(void *buffer, size_t size, size_t nmemb, void *userp);

extern size_t downloadFile(void *buffer, size_t size, size_t nmemb, std::ofstream* userp);

extern long long getFileSize(const string&);

extern std::function<bool(const char&)> yesOrNo;

extern bool runMainOptions(Options&);

extern void runMainProgram(const string&, Downloader&);

#endif // !FUNCTIONS

