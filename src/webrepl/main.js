const React = require('react')
    , ReactDOM = require('react-dom')
    , TreeView = require('react-treeview')
    , jqconsole = require('jq-console')


/* note: comments that look like // / are there to fix
   kak's syntax highlighting (which breaks on JSX) */

const ws = new WebSocket('ws://' + location.host + '/webrepl/socket')
    , term = $('#terminal').jqconsole('Hello from Vio!\n', 'vio> ')

function prompt() {
    term.Prompt(true, ws.send.bind(ws))
}

function getoffset(i, parenti, level) {
    return i + Math.pow(2, parenti) + Math.pow(10, level)
}

const StackView = React.createClass({
    getInitialState() {
        return {stack: [], collapsedNodes: [[]]}
    }

,   handleClick(i, level, parenti) {
        let [...collapsedNodes] = this.state.collapsedNodes
          , idx = getoffset(i, parenti, level)
        collapsedNodes[idx] = !collapsedNodes[idx];
        this.setState({collapsedNodes: collapsedNodes});
    }

,   componentDidMount() {
        ws.onmessage = function (evt) {
            if (evt.data.indexOf("ERROR") == 0) {
                term.Write(evt.data.slice(5) + '\n', 'jqconsole-output')
                this.setState(this.state)
            }
            else {
                const vals = JSON.parse(evt.data)
                term.Write(vals[vals.length - 1].repr + '\n', 'jqconsole-output')
                this.setState({stack: vals, collapsedNodes: Array(vals.length).join(0).split(0).map(() => false)})
            }
            prompt()
        }.bind(this)
    }

,   valTreeView(vals, level, parenti) {
        return vals.map((val, i) => {
            const label = <span className="node" onClick={this.handleClick.bind(this, i)}>{val.what}</span> // /
            return <TreeView
                key={i}
                nodeLabel={label}
                collapsed={this.state.collapsedNodes[getoffset(i, parenti, level)]}
                onClick={this.handleClick.bind(this, i, level, parenti)}>
                <div className="info" key={val.repr}>{val.repr}</div>
                {val.values && val.values.length > 0 ? this.valTreeView(val.values, level + 1, i) : ""}
            </TreeView> // /
        })
    }

,   render() {
        return (
            <div>
                {this.valTreeView(this.state.stack.slice(0).reverse(), 0, 0)}
            </div> // /
        )
    }
})

ReactDOM.render(<StackView />, document.getElementById('stack')) // /

prompt()
