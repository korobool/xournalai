"""Redraw of the owner's paper conspect 'Logistic Regression as a NN' (~/Desktop/LinearRegression.jpg),
same 3-column landscape layout, with corrections:
  - sigmoid: 1/(1 - exp(-z))  ->  1/(1 + exp(-z))                      (bug)
  - np.zeroes -> np.zeros, initialize_with_zeroes -> initialize_with_zeros, np.zeros((1, m)) takes a tuple
  - optimize(): `cost = []` but returned `costs`; costs never appended -> record every 100 iterations (+ print_cost)
  - prediction rule conditioned on y > 0.5 -> a > 0.5
  - J formula: missing brackets around the summand
  - prose typos: Badkward/Propogation, theshold, Garning, Notet, truncated URL, 'NG' -> Ng
Run:  python3 logistic_regression.py OUTDIR   -> OUTDIR/logreg.{svg,latex.json,png}
"""
import os, sys, math
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "lib"))
from conspect import Sheet

out = sys.argv[1] if len(sys.argv) > 1 else "."
s = Sheet(842, 595, seed=11)
T, CODE, SMALLT = 3.9, 4.0, 3.4       # prose / code / annotation sizes (x-height, pt)
LC = 11.0                              # code line spacing

def rrect(x, y, w, h, r=6):  # rounded rectangle = cloud without waves
    s.cloud(x, y, w, h, amp=0.0, radius=r)

# ============================ LEFT COLUMN ============================
s.heading(6, 11, "Logistic Regression", size=4.6)
s.heading(40, 22, "as a NN", size=4.6)
s.text(9, 33, "- consider a single NN:\n  it computes a linear\n  function followed by\n  an activation function", size=3.5, line=10.2)
rrect(10, 72, 72, 30, r=7)
s.latex(15, 75, r"z = w^\top x + b", 9.5, maxw=64)
s.latex(15, 88, r"\hat y = a = \sigma(z)", 11, maxw=64)

s.text(104, 11, "Logistic Loss", size=4.6)
rrect(103, 15, 66, 18, r=5)
s.text(107, 22, "cross-entropy\n  loss function", size=SMALLT, line=7.2)
s.cloud(102, 35, 118, 36)
s.latex(108, 39, r"\mathcal{L}(a,y) =", 10, maxw=108)
s.latex(107, 53, r"-\big(y\log(a) + (1-y)\log(1-a)\big)", 12, maxw=110)

# neuron diagram
for k, (lbl, yy) in enumerate([(r"x_0^{(i)}", 80), (r"x_1^{(i)}", 90), (r"x_n^{(i)}", 110)]):
    s.latex(100, yy, lbl, 8)
s.latex(104, 99.5, r"\vdots", 7)
for (y0, wl, yw) in [(85, r"w_0", 80.5), (95, r"w_1", 91.5), (114, r"w_n", 109.5)]:
    s.arrow((113, y0), (129, 97 + (y0 - 97) * 0.35), head=3)
    s.latex(118, yw, wl, 4.5)
rrect(129, 86, 72, 23, r=11)
s.line((180, 87), (180, 108))
s.latex(133, 92, r"w^\top x^{(i)} + b", 9, maxw=45)
s.latex(185, 92, r"\sigma", 7)
s.arrow((201, 97.5), (212, 97.5), head=3.5)
s.text(214, 91, '"1" if a > 0.5', size=3.3)
s.text(214, 106, '"0" if a ≤ 0.5', size=3.3)

s.text(6, 119, "For one example:", size=T)
s.cloud(4, 123, 112, 33)
s.latex(9, 126, r"z^{(i)} = w^\top x^{(i)} + b", 10, maxw=102)
s.latex(9, 140, r"\hat y^{(i)} = a^{(i)} = \mathrm{sigmoid}(z^{(i)})", 11, maxw=102)
s.cloud(120, 126, 124, 50)
s.latex(125, 131, r"\mathcal{L}(a^{(i)}, y^{(i)}) = -\,y^{(i)}\log(a^{(i)})", 11, maxw=114)
s.latex(152, 152, r"-\,(1-y^{(i)})\log(1-a^{(i)})", 11, maxw=88)
s.text(6, 166, "The cost is computed by\nsumming over all training\nexamples:", size=T, line=9.6)
s.cloud(30, 187, 104, 31)
s.latex(37, 190, r"J = \frac{1}{m}\sum_{i=1}^{m}\mathcal{L}(a^{(i)}, y^{(i)})", 24, maxw=92)
s.line((0, 222), (273, 222))

s.code(6, 234, """def model(X_train, Y_train, X_test, Y_test, num_iterations=2000,
          learning_rate=0.5, print_cost=False):
    w, b = initialize_with_zeros(X_train.shape[0])
    parameters, grads, costs = optimize(w, b, X_train, Y_train,
                 num_iterations, learning_rate, print_cost)
    w = parameters['w']
    b = parameters['b']
    Y_prediction_test = predict(w, b, X_test)
    Y_prediction_train = predict(w, b, X_train)
    print('train accuracy: {} %'.format(100 - np.mean(
          np.abs(Y_prediction_train - Y_train)) * 100))
    print('test accuracy: {} %'.format(100 - np.mean(
          np.abs(Y_prediction_test - Y_test)) * 100))
    d = {'costs': costs,
         'Y_prediction_test': Y_prediction_test,
         'Y_prediction_train': Y_prediction_train,
         'w': w,
         'b': b,
         'learning_rate': learning_rate,
         'num_iterations': num_iterations}
    return d""", size=CODE, line=LC)
s.line((0, 524), (273, 524))
s.code(6, 537, """d = model(train_set_x, train_set_y, test_set_x, test_set_y,
          num_iterations=2000, learning_rate=0.005,
          print_cost=True)""", size=CODE, line=LC)

# ============================ MIDDLE COLUMN ============================
s.line((273, 0), (273, 595))
s.heading(278, 11, "NumPy implementation of Logistic Regression", size=4.8)
s.rect(275, 18, 132, 26)
s.code(279, 28, "def sigmoid(z):\n    return 1 / (1 + np.exp(-z))", size=CODE, line=10.5)
s.brace(408, 19, 43, side="right", depth=5)

s.rect(275, 45, 156, 47)
s.code(279, 55, "def initialize_with_zeros(dim):\n    w = np.zeros(shape=(dim, 1))\n    b = 0\n    return w, b", size=CODE, line=10.5)

s.rect(275, 93, 268, 124)
s.code(279, 103, """def propagate(w, b, X, Y):
    m = X.shape[1]
    A = sigmoid(np.dot(w.T, X) + b)
    cost = (-1/m) * np.sum(Y*np.log(A) + (1-Y)*np.log(1-A))
    dw = 1/m * np.dot(X, (A - Y).T)
    db = 1/m * np.sum(A - Y)
    cost = np.squeeze(cost)
    grads = {'dw': dw,
             'db': db}
    return grads, cost""", size=CODE, line=LC)
s.bracket(286, 118, 139, label="FP", size=3.0)
s.bracket(286, 140, 161, label="BP", size=3.0)
s.text(452, 101, "forward propagation\n   (to find cost)", size=SMALLT, line=7.5)
s.polyline([(433, 121), (470, 115), (520, 116), (540, 121)], wobble=True)
s.brace(430, 139, 172, side="left", depth=5)
s.text(436, 150, "backward\npropagation\n(to find grad)", size=SMALLT, line=7.5)
s.brace(545, 116, 139, side="right", depth=4)
s.brace(545, 140, 162, side="right", depth=4)

s.rect(275, 218, 268, 193)
s.code(279, 228, """def optimize(w, b, X, Y, num_iterations, learning_rate,
             print_cost=False):
    costs = []
    for i in range(num_iterations):
        grads, cost = propagate(w, b, X, Y)
        dw = grads['dw']
        db = grads['db']
        w = w - learning_rate * dw
        b = b - learning_rate * db
        if i % 100 == 0:
            costs.append(cost)
            if print_cost:
                print('cost after iteration %i: %f' % (i, cost))
    params = {'w': w, 'b': b}
    grads = {'dw': dw, 'db': db}
    return params, grads, costs""", size=CODE, line=LC)
s.brace(545, 300, 322, side="right", depth=4)

s.rect(275, 412, 268, 103)
s.code(279, 422, """def predict(w, b, X):
    m = X.shape[1]
    Y_prediction = np.zeros((1, m))
    w = w.reshape(X.shape[0], 1)
    A = sigmoid(np.dot(w.T, X) + b)
    for i in range(A.shape[1]):
        Y_prediction[0, i] = 1 if A[0, i] > 0.5 else 0
    return Y_prediction""", size=CODE, line=LC)
s.brace(545, 440, 505, side="right", depth=5)

s.text(279, 528, "See  dennybritz/nn-from-scratch  on GitHub\nwww.wildml.com/2015/09/implementing-a-neural-network-from-scratch/",
       size=3.6, line=10.5)
b = s.text(279, 551, "Note:", size=3.6)
s.line((b[0], 553), (b[2], 553), width=0.6)
s.text(b[2] + 4, 551, "Normalization is needed to keep a common scale of the\nlearning rate for training: that way we only need",
       size=3.6, line=10.5)
s.text(330, 574, "ONE GLOBAL LEARNING RATE MULTIPLIER", size=3.4)

# ============================ RIGHT COLUMN ============================
s.text(566, 27, "Sigmoid in this case will be computing this:", size=T)
s.cloud(570, 31, 232, 28)
s.latex(575, 33, r"\mathrm{sigmoid}(z) = \mathrm{sigmoid}(w^\top x + b) = \frac{1}{1+e^{-(w^\top x+b)}}", 22, maxw=222)
s.connector([(414, 31), (470, 31), (570, 44)])

s.text(566, 95, "Forward Propagation computes:", size=4.2)
s.rect(568, 99, 228, 67)
s.latex(572, 103, r"A = \sigma(w^\top X + b) = (a^{(1)}, a^{(2)}, \dots, a^{(m)})", 12, maxw=218)
s.text(573, 131, "and  COST FUNCTION:", size=T)
s.latex(572, 136, r"J = -\frac{1}{m}\sum_{i=1}^{m}\big[y^{(i)}\log(a^{(i)}) + (1-y^{(i)})\log(1-a^{(i)})\big]", 22, maxw=220)
s.brace(798, 100, 122, side="right", depth=4); s.text(805, 114, "activation", size=3.3)
s.brace(798, 136, 162, side="right", depth=4); s.text(805, 152, "cost", size=3.3)
s.connector([(550, 127), (568, 127)])

s.text(566, 177, "Backward Propagation computes:", size=4.2)
s.cloud(568, 182, 142, 60)
s.latex(574, 186, r"\frac{\partial J}{\partial w} = \frac{1}{m}\, X (A-Y)^\top", 20, maxw=132)
s.latex(574, 212, r"\frac{\partial J}{\partial b} = \frac{1}{m}\sum_{i=1}^{m}\big(a^{(i)} - y^{(i)}\big)", 26, maxw=132)
s.brace(714, 186, 207, side="right", depth=4); s.text(724, 199, "dw", size=T)
s.brace(714, 212, 238, side="right", depth=4); s.text(724, 228, "db", size=T)
s.connector([(550, 151), (558, 151), (568, 212)])

s.text(569, 256, "Optimization goal is to learn w & b by\nminimizing the cost function J. For a parameter θ\nthe update rule is:", size=T, line=10.2)
s.cloud(568, 289, 198, 25)
s.latex(575, 294, r"\theta = \theta - \alpha\, d\theta \qquad (\alpha \text{ -- learning rate})", 14, maxw=186)
s.connector([(550, 311), (568, 302)])

b = s.text(566, 330, "Prediction calculates labels", size=4.1)
s.latex(b[2] + 3, 321, r"\hat Y", 10)
s.text(b[2] + 14, 330, "and using a", size=4.1)
s.text(566, 343, "threshold (= 0.5) makes a decision: is the answer 0 or 1", size=3.7)
s.cloud(568, 350, 138, 78)
s.latex(574, 355, r"\hat Y = A = \sigma(w^\top X + b)", 13, maxw=128)
s.latex(574, 376, r"\begin{cases} y^{(i)} \to 1 & \text{if } a^{(i)} > 0.5 \\ y^{(i)} \to 0 & \text{if } a^{(i)} \le 0.5 \end{cases}", 36, maxw=128)
s.connector([(551, 472), (560, 472), (568, 400)])

# sigmoid plot
s.cloud(709, 357, 130, 73, amp=1.2)
s.text(727, 372, "Sigmoid:", size=3.6)
ox, oy = 777, 423
s.arrow((721, oy), (834, oy), head=3.5); s.arrow((ox, oy), (ox, 377), head=3.5)
z = np.linspace(-8.3, 8.3, 120)
s.polyline(np.c_[ox + z * 6.4, oy - 38 / (1 + np.exp(-z))], width=1.4)
for xx in range(724, 834, 6): s.line((xx, oy - 38), (xx + 3, oy - 38), width=0.5)
s.text(766, oy - 36, "1.0", size=2.4); s.text(766, oy - 17, "0.5", size=2.4)
for v in (-8, -4, 4, 8): s.text(ox + v * 6.4 - 2, oy + 7, str(v), size=2.4); s.line((ox + v * 6.4, oy - 1.5), (ox + v * 6.4, oy + 1.5), width=0.5)

# cost-function panel (wavy left edge like the original)
s.polyline([(576 + 1.2 * math.sin(t / 6), t) for t in np.arange(432, 592, 2)], wobble=False)
s.text(590, 441, "Cost Function", size=3.5)
cx0, cy0 = 590, 490
s.arrow((cx0, cy0), (662, cy0), head=3.5); s.arrow((cx0, cy0), (cx0, 445), head=3.5)
it = np.linspace(0, 1, 60); s.polyline(np.c_[cx0 + 2 + it * 64, cy0 - 3 - 38 * np.exp(-3.2 * it)], width=1.1)
s.text(579, 450, "0.7", size=2.4); s.text(579, 492, "0.1", size=2.4)
for k, v in enumerate(("5", "10", "15")): s.text(603 + 14 * k, 498, v, size=2.4)
s.text(641, 498, "# iterations", size=2.4)

s.text(694, 441, "Different Learning Rates:", size=3.5)
lx, ly = 697, 494
s.arrow((lx, ly), (776, ly), head=3.5); s.arrow((lx, ly), (lx, 447), head=3.5)
s.polyline([(699, 452), (715, 457), (740, 462), (768, 466)], width=1.0, wobble=True)
s.polyline([(699, 452), (705, 460), (711, 470), (716, 463), (722, 474), (745, 478), (768, 480)], width=1.0, wobble=True)
s.polyline([(699, 452), (703, 468), (707, 478), (711, 471), (716, 484), (740, 488), (768, 490)], width=1.0, wobble=True)
s.text(771, 468, "0.0001", size=2.6); s.text(771, 482, "0.001", size=2.6); s.text(771, 492, "0.01", size=2.6)

s.text(589, 517, "See Andrew Ng's course\nNeural Networks and Deep Learning", size=4.1, line=12)

svg, man = s.save(os.path.join(out, "logreg"), dpi=170)
s.save_xopp(os.path.join(out, "logreg.xopp"))
print(svg, man, len(s.paths), "paths,", len(s.latexes), "formulas")
