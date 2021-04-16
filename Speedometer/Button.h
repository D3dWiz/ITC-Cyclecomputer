class Button {

  private:
  
    bool _state;
    uint8_t _pin;

  public:
  
    Button(uint8_t pin) : _pin(pin) {}

    void begin() {
      pinMode(_pin, INPUT_PULLUP);
      _state = digitalRead(_pin);
    }

    bool isReleased() {
      bool v = digitalRead(_pin);
      if (v != _state) {
        delay(50);
        _state = v;
        if (_state) {
          delay(50);
          return true;
        }
      }
      return false;
    }

};
