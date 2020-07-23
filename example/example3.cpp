#include <iostream>
#include <string.h>

using namespace std;

void enable_trusted_code_execution() {
	return;
}

void disable_trusted_code_execution() {
	return;
}

string encryption(string plain_text) {
	enable_trusted_code_execution();
	string cipher_text = plain_text;
	for (int i=0;i<cipher_text.length();i++) {
		cipher_text[i]++;
	}
	disable_trusted_code_execution();
	return cipher_text;
}

string decryption(string cipher_text) {
	enable_trusted_code_execution();
	string plain_text = cipher_text;
	for (int i=0;i<plain_text.length();i++) {
		plain_text[i]--;
	}
	disable_trusted_code_execution();
	return plain_text;
}

string get_msg() {
	string msg = "hello dixit how are you!";
	return msg;
}


int main () {
	for (int i=0;i<5000;i++) {
		string msg = get_msg();
		msg = encryption(msg);
		cout << "Cipher-text: " << msg << endl; 
		msg = decryption(msg);
		cout << "Plain-text: " << msg << endl;
	}
	return 0;
}


