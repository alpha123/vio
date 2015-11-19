const React = require('react')
    , ReactDOM = require('react-dom')
    , TreeView = require('react-treeview')
    , jqconsole = require('jq-console')


/* note: comments that look like // / are there to fix
   kak's syntax highlighting (which breaks on JSX) */

const ws = new WebSocket('ws://127.0.0.1:' + location.port + '/webrepl/socket')
    , term = $('#terminal').jqconsole('Hello from Vio!\n', 'vio> ')

function prompt() {
    term.Prompt(true, ws.send.bind(ws))
}

const StackView = React.createClass({
    getInitialState() {
        return {stack: [], collapsedNodes: []}
    }

,   handleClick(i) {
        let [...collapsedNodes] = this.state.collapsedNodes
        collapsedNodes[i] = !collapsedNodes[i];
        this.setState({collapsedNodes: collapsedNodes});
    }

,   componentDidMount() {
        ws.onmessage = function (evt) {
            if (evt.data.indexOf("ERROR") == 0) {
                term.write(evt.data + '\n', 'jqconsole-output')
            }
            else {
                const vals = JSON.parse(evt.data)
                term.Write(vals[vals.length - 1].repr + '\n', 'jqconsole-output')
                this.setState({stack: vals})
            }
            prompt()
        }.bind(this)
    }

,   valTreeView(vals) {
        return vals.map((val, i) => {
            const label = <span className="node" onClick={this.handleClick.bind(this, i)}>{val.what}</span> // /
            return <TreeView
                key={i}
                nodeLabel={label}
                collapsed={this.state.collapsedNodes[i]}
                onClick={this.handleClick.bind(this, i)}>
                <div className="info" key={val.repr}>{val.repr}</div>
                {val.values && val.values.length > 0 ? this.valTreeView(val.values) : ""}
            </TreeView> // /
        })
    }

,   render() {
        return (
            <div>
                {this.valTreeView(this.state.stack.reverse())}
            </div> // /
        )
    }
})

ReactDOM.render(<StackView />, document.getElementById('stack')) // /

prompt()
