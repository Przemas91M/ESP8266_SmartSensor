# ESP8266_SmartSensor
Projekt wykonany w oparciu o mikroprocesor ESP8266.
Po uruchomieniu urządzenie łączy się z zapisaną siecią Wi-Fi.
Jeżeli nie może nawiązać połączenia, przechodzi w tryb Acces Point.
Po połączeniu się z urządzeniem np za pomocą telefonu - wyświetli się strona z ustawieniami, gdzie można wpisać dane sieci.

Gdy mamy już ustabilizowane połaczenie z siecią Wi-Fi, urządzenie wysyła temperaturę oraz wilgotność odczytaną z czujnika BME280 do serwera MQTT.
