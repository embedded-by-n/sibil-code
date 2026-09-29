"""Check that the dock's ML tools are installed and the TTM weights load.

Run with:  uv run check_models.py
"""

import lightgbm
import numpy as np
import sklearn
import torch
from tsfm_public import TinyTimeMixerForPrediction

TTM_MODEL = "ibm-granite/granite-timeseries-ttm-r2"


def main() -> None:
    print(f"scikit-learn {sklearn.__version__}, LightGBM {lightgbm.__version__}, torch {torch.__version__}")

    # Stage 1: a LightGBM classifier on made-up window features.
    rng = np.random.default_rng(0)
    X = rng.normal(size=(200, 4))
    y = (X[:, 0] + X[:, 1] > 0).astype(int)
    clf = lightgbm.LGBMClassifier(n_estimators=20, verbose=-1).fit(X, y)
    print(f"LightGBM fit ok, training accuracy {clf.score(X, y):.2f}")

    # Stage 2: TinyTimeMixer, downloaded from Hugging Face on first run.
    model = TinyTimeMixerForPrediction.from_pretrained(TTM_MODEL)
    cfg = model.config
    params = sum(p.numel() for p in model.parameters())
    print(f"TTM loaded: {params / 1e6:.1f}M parameters, "
          f"context {cfg.context_length} steps -> forecast {cfg.prediction_length} steps")

    # Forecast four made-up sensor channels (sweat, heart rate, temp, motion).
    past = torch.randn(1, cfg.context_length, 4)
    with torch.no_grad():
        out = model(past_values=past)
    print(f"TTM forecast ok, output shape {tuple(out.prediction_outputs.shape)}")


if __name__ == "__main__":
    main()
