#pragma once

class IBrightnessController {
public:
    virtual ~IBrightnessController() = default;

    virtual bool AnyControllable() const = 0;
    virtual void Capture() = 0;
    virtual void Restore() = 0;
    virtual bool SetPercent(int percent) = 0;
    virtual int GetCurrentPercent() const { return -1; }
};
