#pragma once
#include <iostream>
#include <vector>
#include <random>
#include <cstdint>

using namespace std;

struct Encryption{
    vector <uint8_t> generate_key(){
        random_device rd; //создает источник случайности
        mt19937 gen(rd()); //создает сам генератор, получаем сид
        uniform_int_distribution<int> dist(0,255); //обхект, превращающий сырые числа в распределенные в диапазоне 0-255

        vector <uint8_t> key(32);

        for(int i = 0; i < 32; i++){
            //key.push_back(dist(gen)); //dist создает новое случайное число
            key[i] = static_cast<uint8_t>(dist(gen));
        }

        return key;
    }

    vector <uint8_t> gamma(vector <uint8_t> data, vector <uint8_t> key){
        for(int i = 0; i < data.size(); i++){
            data[i] ^= key[i % key.size()];
        }
        return data;
    }
};