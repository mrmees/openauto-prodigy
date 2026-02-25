import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from match_loader import load_matches


def test_load_matches():
    """Load and merge auto-matches with manual overrides."""
    matches = load_matches()
    assert "ChannelDescriptorData" in matches or "ChannelDescriptor" in matches
    for _, info in list(matches.items())[:1]:
        assert "apk_class" in info
        assert "score" in info
        assert "source" in info


def test_manual_override():
    """Manual overrides should take precedence."""
    matches = load_matches()
    wbw_match = None
    for _, info in matches.items():
        if info["apk_class"] == "wbw":
            wbw_match = info
            break
    assert wbw_match is not None
    assert wbw_match["source"] == "manual"


if __name__ == "__main__":
    test_load_matches()
    test_manual_override()
    print("All match_loader tests passed")
