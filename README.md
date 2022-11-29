# Monitoramento Remoto de Sinais Vitais

Este projeto faz parte do trabalho **Sistema de Baixo Custo para Monitoramento Remoto de Sinais Vitais**, apresentado à UNIP (Universidade Paulista) para conclusão do curso de Ciência da Computação.

O código é utilizado na placa de desenvolvimento ESP32, que controla um aparelho de pressão arterial e sensores para coletar os sinais vitais do paciente e compartilhá-los em tempo real com um servidor MQTT.

<p align="center"><img src="https://raw.githubusercontent.com/Andreick/ESP32-health-monitor/assets/imgs/prototype.jpg" alt="Sensores com o ESP32 em uma protoboard" height="300"></p>

<p align="center"><img src="https://raw.githubusercontent.com/Andreick/ESP32-health-monitor/assets/imgs/visao_global.png" alt="Representação do sistema" height="400"></p>

## Captura dos Sinais Vitais

Os sinais vitais monitorados são a pressão arterial, a frequência cardíaca, a saturação de oxigênio do sangue e a temperatura corporal.

A pressão arterial é obtida utilizando o aparelho LP200 Premium. Ao conectar alguns fios ao ESP32 foi possível obter as medições, todo o processo está detalhado no projeto [blood-pressure-monitor-hack](https://github.com/Andreick/blood-pressure-monitor-hack).

<p align="center"><img src="https://raw.githubusercontent.com/Andreick/ESP32-health-monitor/assets/imgs/LP200.jpg" alt="LP200 conectado ao ESP32 em uma protoboard" height="300"></p>

A frequência cardíaca e a saturação de oxigênio são coletadas pelo módulo MAX30102, que foi inserido em um clipe de dedo. O algoritmo para o cálculo dos sinais é o mesmo utilizado no projeto [aromring/MAX30102_by_RF](https://github.com/aromring/MAX30102_by_RF).

<p align="center"><img src="https://raw.githubusercontent.com/Andreick/ESP32-health-monitor/assets/imgs/MAX30102.jpg" alt="MAX30102 em clipe de dedo" height="300"></p>

A temperatura é obtida pelo sensor DS18B20, que deve ser posicionado na axila.

<p align="center"><img src="https://raw.githubusercontent.com/Andreick/ESP32-health-monitor/assets/imgs/DS18B20.jpg" alt="DS18B20 conectado ao ESP32 em uma protoboard" height="300"></p>

## Envio pela Internet

O ESP32 é conectado à um servidor MQTT remoto hospedado na Digital Ocean. A implementação do broker é feita com o Mosquitto.

No servidor também foi instalado o Node-Red para criar um painel que exibe os sinais monitorados.

<p align="center"><img src="https://raw.githubusercontent.com/Andreick/ESP32-health-monitor/assets/imgs/painel.png" alt="Painel criado com o Node-Red" height="300"></p>