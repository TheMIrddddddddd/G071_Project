/**
 * @file animate_sequence.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2025-04-04
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "animate_sequence.hpp"
#include <algorithm>
#include <memory>

using namespace smooth_ui_toolkit;

AnimateSequence::operator float()
{
    return value();
}

void AnimateSequence::play()
{
    if (_animate_value && !_is_done) {
        _is_paused = false;
        if (_animate_value->currentPlayingState() == AnimateState::Paused) {
            _animate_value->play();
        }
        return;
    }
    // Reset index and create animate value
    if (!_value_sequence.empty()) {
        _current_index = 0;
        _current_value = _value_sequence[_current_index];
        if (!_animate_value) {
            _animate_value = std::make_unique<AnimateValue>(_current_value);
        }
        _animate_value->teleport(_current_value);
        if (_on_play) {
            _on_play(*_animate_value);
        }
        _animate_value->init();
        _animate_value->complete(); // Initial keyframe has no transition duration.
        _repeat_count = repeat;
        _is_done = false;
        _is_paused = false;
    }
}

void AnimateSequence::pause()
{
    if (!_is_done) { _is_paused = true; }
    if (_animate_value) {
        _animate_value->pause();
    }
}

void AnimateSequence::complete()
{
    if (!_value_sequence.empty()) {
        _current_value = _value_sequence.back();
    }
    if (_animate_value) { _animate_value->cancel(); }
    _is_done = true;
    _is_paused = false;
    if (_on_done) {
        _on_done();
    }
}

void AnimateSequence::cancel()
{
    if (!_value_sequence.empty()) {
        _current_value = _value_sequence.front();
    }
    if (_animate_value) { _animate_value->cancel(); }
    _is_done = true;
    _is_paused = false;
    if (_on_done) {
        _on_done();
    }
}

void AnimateSequence::update()
{
    // If animate value instance exists
    if (_animate_value && !_is_done && !_is_paused &&
        _animate_value->currentPlayingState() != AnimateState::Paused) {
        _current_value = _animate_value->value();
        // If animation is done, retarget to next value
        if (_animate_value->done()) {
            int next_index = _current_index + 1;
            if (next_index < _value_sequence.size()) {
                _current_index = next_index;
                _animate_value->stop();
                if (_on_step) {
                    _on_step(*_animate_value, _value_sequence, _current_index);
                }
                _animate_value->begin();
                _animate_value->cancel();
                _animate_value->teleport(_current_value);
                _animate_value->move(_value_sequence[_current_index]);
            }
            // If reached the end of the sequence
            else {
                // If repeat is not set, stop the animation
                if (_repeat_count == 0) {
                    if (_animate_value) { _animate_value->cancel(); }
                    _is_done = true;
                    if (_on_done) {
                        _on_done();
                    }
                    return;
                }
                // Count down repeat
                if (_repeat_count > 0) {
                    _repeat_count--;
                }
                // Reset animation
                if (repeatType == AnimateRepeatType::Reverse) {
                    std::reverse(_value_sequence.begin(), _value_sequence.end());
                }
                _current_index = 0;
                _current_value = _value_sequence[_current_index];
                _animate_value->teleport(_current_value);
                _animate_value->complete(); // No delay at the repeated initial keyframe.
            }
        }
    }
}

float AnimateSequence::value()
{
    update();
    return _current_value;
}
